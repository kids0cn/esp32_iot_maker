#pragma once
/*
 * PM2.5 粉尘传感器模块 —— Sharp GP2Y1014AU + 转接板（4 线制）
 * ============================================
 * 和 light.h 一个套路：main.cpp 只负责「装配」，本模块管自己那一摊。
 * 固定三样：常量 + pm25Init() + pm25Read()。
 *
 * ── 接线（4 根线，转接板已含全部元件，不用外接任何东西）──
 *   转接板 VCC  → ESP32 **VIN / 5V**     ⚠️ 5V，不是 3.3V
 *   转接板 GND  → ESP32 GND
 *   转接板 AO   → **GPIO32**             ADC1_CH4
 *   转接板 ILED → **GPIO14**
 *
 *   板上自带：150Ω + 100µF×2（供电 RC，手册要求）、10k/1k（分压）。
 *   板上还有一颗电源指示灯 D1 —— 亮 = 板内 5V 正常。
 *
 * ── 三个关键参数，都是实测或手册来的，不是猜的 ──
 *
 *   ① LED_ON_LEVEL = HIGH
 *      ILED 经板上 **SS8050 三极管**驱动，**反相** —— 高电平才点亮。
 *      （网上流传的那份 Arduino 示例写 `digitalWrite(ledPower, LOW)` 点亮，
 *        那是**直连**、没有三极管。照抄到这块转接板上 LED 根本不会亮。）
 *
 *   ② DIVIDER_RESTORE = 11
 *      板上 1k/10k 分压，AO 上只有传感器真实输出的 1/11，乘 11 还原。
 *
 *   ③ NO_DUST_MV = 1562
 *      零点校准值 = 实测干净空气下 AO(142mV) × 11。
 *      手册明确说每枚传感器无尘输出有个体差异，**必须逐台校准**。
 *      换传感器、或想在更准的环境下用，就重校一次：
 *        拿到通风处/户外读一次 Vo=，把那个数填回来。
 *
 * ── 读数怎么来的（这行是重点，我们在这里踩了整整一轮坑）──
 *   用 **analogReadMilliVolts()**，**不用** analogRead() × 3.3/4095。
 *   ESP32 的 analogRead() 在低压段（<0.1V）分辨率极差、经常直接返回 0，
 *   而干净空气下 AO 只有几十 mV —— 用 analogRead() 会一直读到 0，
 *   看上去就像「传感器坏了 / 没接 / 没输出」，实际它一直好好的。
 *   analogReadMilliVolts() 会读芯片出厂烧在 efuse 里的校准系数补偿非线性，
 *   低压段能正常读数。这是 ESP32 专用读法。
 *
 * ── ⚠️ pm25Read() 会阻塞约 10ms ────────────────
 *   它必须等够 LED 的 10ms 脉冲周期（手册要求）。
 *   所以**绝对不能在 sendState() 里调** —— 那会把网页请求堵住。
 *   正确做法：在 loop() 里每 1 秒调一次，把结果存起来，请求时用缓存值。
 *   （每秒阻塞 10ms ≈ 1%，handleClient() 一秒被调上千次，无感。）
 *
 * ── 读数是「相对值」────────────────────────────
 *   零点按你所在环境校的，斜率用手册标称灵敏度，**未经专业仪器标定**。
 *   判高低、看变化、做联动 —— 够用；当仪器读数 —— 不行。
 */

#include <Arduino.h>

// ================== 常量：本模块全部参数都在这 ==================
const int PM25_LED_PIN  = 14;      // 接转接板 ILED
const int PM25_AN_PIN   = 32;      // 接转接板 AO（必须在 ADC1：32/33/34/35/36/39）
const int LED_ON_LEVEL  = HIGH;    // ★ 转接板有三极管反相 → HIGH 点亮
const int LED_OFF_LEVEL = LOW;

// 采样时序 —— 依据 Sharp 手册「Recommended input condition for LED」
//   脉冲周期 T  = 10 ± 1 ms      ← 三段加起来必须凑够 10ms
//   脉冲宽度 PW = 0.32 ± 0.02 ms ← 即 320µs
//   采样时点    = 脉冲开始后 0.28 ms
const int SAMPLING_US   = 280;     // 点亮后等 280µs 再采样
const int PULSE_TAIL_US = 40;      // 采样完再亮 40µs，凑满 320µs 脉宽
const int SLEEP_US      = 9680;    // 补足 10ms（280 + 40 + 9680 = 10000µs）

// 浓度换算（⚠️ 相对值，未经仪器标定）
const float ADC_MAX         = 4095.0;   // ESP32 analogRead 12 位
const float ADC_VREF_MV     = 3300.0;   // 参考电压
const float DIVIDER_RESTORE = 11.0;     // 板上 10k/1k 分压 → 乘 11 还原
const float NO_DUST_MV      = 1562.0;   // ★ 零点：实测干净空气 AO 142mV × 11
const float COV_RATIO       = 0.20f;    // µg/m³ per mV（手册 K=0.5V/100µg/m³）
                                        // 标准值，不用改

// 10 次滑动平均，滤掉环境杂散光和电源纹波（厂商例程也这么做）
const int PM25_FILTER_N = 10;

// ================== 初始化：setup() 里调一次 ==================
inline void pm25Init() {
  pinMode(PM25_LED_PIN, OUTPUT);
  digitalWrite(PM25_LED_PIN, LED_OFF_LEVEL);   // 先熄灭，别开机就白亮
  pinMode(PM25_AN_PIN, INPUT);
}

// ================== 内部：滑动平均 ==================
// 用**函数内静态变量**，不用全局 —— 这样即使头文件被多个文件 include，
// 也不会产生重复定义的符号。
inline int pm25FilterAvg(int sample) {
  static int  buf[PM25_FILTER_N];
  static int  idx = 0;
  static long sum = 0;
  static bool filled = false;

  if (!filled) {
    for (int i = 0; i < PM25_FILTER_N; i++) buf[i] = sample;
    sum = (long)sample * PM25_FILTER_N;
    idx = 0;
    filled = true;
    return sample;
  }
  sum -= buf[idx];
  buf[idx] = sample;
  sum += buf[idx];
  idx = (idx + 1) % PM25_FILTER_N;
  return (int)(sum / PM25_FILTER_N);
}

// ================== 读取：loop() 里每秒调一次 ==================
// 返回值：PM2.5 浓度（µg/m³，相对值）
// voltageMvOut（可选）：还原分压后的传感器原始输出 VO(mV)，调试用
//
// ⚠️ 本函数阻塞约 10ms，不要在 sendState() 里调。
inline int pm25Read(float* voltageMvOut = nullptr) {
  digitalWrite(PM25_LED_PIN, LED_ON_LEVEL);        // 点亮内部红外 LED
  delayMicroseconds(SAMPLING_US);                  // 等 280µs 让光路稳定
  int pinMv = (int)analogReadMilliVolts(PM25_AN_PIN);  // ★ 校准读法，低压段才读得出来
  delayMicroseconds(PULSE_TAIL_US);                // 再亮 40µs，凑满 320µs
  digitalWrite(PM25_LED_PIN, LED_OFF_LEVEL);       // 熄灭
  delayMicroseconds(SLEEP_US);                     // 补足 10ms 周期

  int mvFiltered = pm25FilterAvg(pinMv);

  // 还原分压 → 传感器真实输出 VO(mV)
  float vo = mvFiltered * DIVIDER_RESTORE;
  if (voltageMvOut) *voltageMvOut = vo;

  // 电压 → 浓度
  float density = 0;
  if (vo > NO_DUST_MV) density = (vo - NO_DUST_MV) * COV_RATIO;
  return (int)density;
}
