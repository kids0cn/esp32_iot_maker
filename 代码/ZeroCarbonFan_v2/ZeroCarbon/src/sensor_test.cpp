/*
 * PM2.5 粉尘传感器测试（Sharp GP2Y1014AU）
 * ============================================
 * 独立测试程序：只读传感器、往串口打印，不开 WiFi、不碰继电器。
 * 用来确认「接线对不对、传感器活没活、读数会不会动」。
 *
 * ── 怎么读串口输出 ────────────────────────────
 *   每秒一行：
 *     raw=1234  V=0.99V  PM2.5=65 ug/m3   [min=58 max=71]
 *   · raw 长期恒为 0      → 传感器没输出，检查 5 脚接线 / 供电
 *   · raw 长期恒为 4095   → 输出超量程或接错脚，检查分压
 *   · raw 在一个窄区间抖  → 传感器活着，但空气确实干净
 *   · 吹气 / 点蚊香 / 打火机烟后 raw 明显上去 → 一切正常 ✅
 *
 * ── 关键：引脚和换算为什么不是图片里那份 ────────
 *   仓库里的测试代码截图（粉尘传感器测试代码3.jpg）是给 **Arduino AVR** 写的，
 *   直接搬到 ESP32 有四处错，本程序按参考固件修正：
 *
 *     截图写法            问题                        本程序
 *     dustPin=0           GPIO0 是 BOOT 脚，会起不来   PM25_AN_PIN = 34
 *     ledPower=2          GPIO2 是 strapping 脚        PM25_LED_PIN = 13
 *     dustVal/1024        按 10 位算，ESP32 是 12 位    ADC_MAX = 4095
 *     Serial.begin(9600)  和 monitor_speed 不符→乱码   115200
 *   时序 offTime=9680 那部分截图是对的，照抄。
 *
 * ── 接线（依据 文档/传感器/粉尘传感器测试代码2.jpg 的引脚图）──
 *   传感器 1脚 VCC  → 5V        ← 必须 5V，不能接 3.3V
 *   传感器 2脚 GND  → GND
 *   传感器 3脚 LED  → GPIO13    ← 由本程序驱动，低电平点亮
 *   传感器 4脚 GND  → GND
 *   传感器 5脚 OUT  → GPIO34    ← **必须经过 10k 分压**，否则 5V 输出会打坏 ADC
 *   传感器 6脚 VCC  → 5V
 *   另外手册要求 VCC–GND 之间并一颗 220uF 电解电容（物料里有配）。
 *
 * ── 图2 那张空气质量分级表为什么没放进代码 ──────
 *   表里的 0-75 / 75-150 / 150-300 / 300-1050 / 1050-3000 是配**截图那份公式**
 *   （(raw/1024-0.0356)*4200，量程到 4000 多）用的。
 *   本程序用的是参考固件那套换算，量程只到 500 左右，两套量纲不同，
 *   直接套会把「非常好」判成「差」。要统一得先标定，现在没条件，先不套。
 */

#include <Arduino.h>

// ================== 引脚 ==================
const int PM25_LED_PIN = 13;   // 传感器 3 脚：LED 驱动（低电平点亮）
const int PM25_AN_PIN  = 34;   // 传感器 5 脚：模拟输出。GPIO34 是只读输入脚，正合适

// ================== 采样时序 ==================
// 依据 Sharp GP2Y1014AU 手册「Recommended input condition for LED」：
//   脉冲周期 T   = 10 ± 1 ms   ← 三段延时加起来必须凑够 10ms
//   脉冲宽度 PW  = 0.32 ± 0.02 ms ← 即 320us
//   采样时点     = 脉冲开始后 0.28 ms ← 即 280us
// 手册 Note 3 要求按推荐条件驱动 LED，超规格长期跑会影响 LED 寿命。
const int SAMPLING_US    = 280;   // 点亮后等 280us 再采样
const int PULSE_TAIL_US  = 40;    // 采样完再亮 40us，凑满 320us 脉宽
const int SLEEP_US       = 9680;  // 补足 10ms（280 + 40 + 9680 = 10000us）
// ★ 原版固件这里写的是 968，少一位 → 周期只有 1288us，比手册快 7.8 倍。
//   本程序用修正后的 9680。

// ================== 浓度换算 ==================
// ⚠️ 未经标定，输出是**相对值**：判高低够用，不能当仪器读数。
const float ADC_MAX            = 4095.0;   // ESP32 analogRead 默认 12 位（不是 1024！）
const float ADC_VREF           = 3.3;      // ESP32 ADC 参考电压
const float DIVIDER_RESTORE    = 2.0;      // 硬件 10k 分压使电压减半，软件乘 2 还原
const float VOLTAGE_OFFSET     = 0.6;      // 无尘时输出电压（手册 TYP=0.5 / MAX=0.65）
const float VOLTAGE_TO_UGM3    = 166.67;   // 电压→浓度系数（手册 TYP 折算应为 200）

// ================== 读一次 ==================
// 会阻塞约 10ms（必须等够脉冲周期）。每秒读一次完全无所谓；
// 但这个函数不能放进要频繁 handleClient() 的循环里。
int readPM25(int& rawOut) {
  digitalWrite(PM25_LED_PIN, LOW);      // 点亮红外 LED（低电平有效）
  delayMicroseconds(SAMPLING_US);       // 等 280us，让光路稳定
  rawOut = analogRead(PM25_AN_PIN);     // 在脉冲开始后 280us 处采样
  delayMicroseconds(PULSE_TAIL_US);     // 再亮 40us，凑满 320us 脉宽
  digitalWrite(PM25_LED_PIN, HIGH);     // 熄灭 LED
  delayMicroseconds(SLEEP_US);          // 补足 10ms 周期

  float voltage = rawOut * (ADC_VREF / ADC_MAX) * DIVIDER_RESTORE;
  float density = (voltage - VOLTAGE_OFFSET) * VOLTAGE_TO_UGM3;
  if (density < 0) density = 0;         // 干净空气下算出负数，钳到 0
  return (int)density;
}

// ================== 初始化 ==================
void setup() {
  // 115200 必须和 platformio.ini 的 monitor_speed 一致，否则是乱码
  Serial.begin(115200);

  pinMode(PM25_LED_PIN, OUTPUT);
  digitalWrite(PM25_LED_PIN, HIGH);     // 先熄灭，别在启动时白白点亮 LED
  pinMode(PM25_AN_PIN, INPUT);

  Serial.println();
  Serial.println("=== GP2Y1014AU PM2.5 传感器测试 ===");
  Serial.print("  LED 驱动脚 = GPIO");
  Serial.print(PM25_LED_PIN);
  Serial.print("    模拟输入脚 = GPIO");
  Serial.println(PM25_AN_PIN);
  Serial.println("  预期：raw 每秒刷新，吹烟后应明显上升");
  Serial.println();
}

// ================== 主循环 ==================
// 这里用 delay(1000) 是安全的 —— 本程序没有任何网络要服务，
// delay() 只在 WebServer 那种要频繁 handleClient() 的场景才是坑。
void loop() {
  int raw = 0;
  int pm25 = readPM25(raw);
  float voltage = raw * (ADC_VREF / ADC_MAX) * DIVIDER_RESTORE;

  // 记录运行区间，方便判断「读数到底会不会动」
  static int minRaw = 4095, maxRaw = 0;
  if (raw < minRaw) minRaw = raw;
  if (raw > maxRaw) maxRaw = raw;

  Serial.print("raw=");
  Serial.print(raw);
  Serial.print("  V=");
  Serial.print(voltage, 2);
  Serial.print("V  PM2.5=");
  Serial.print(pm25);
  Serial.print(" ug/m3   [min=");
  Serial.print(minRaw);
  Serial.print(" max=");
  Serial.print(maxRaw);
  Serial.println("]");

  // 每 10 秒给一句提示，免得看着一屏数字不知道该干嘛
  static int count = 0;
  if (++count % 10 == 0) {
    Serial.println("  ↑ 试试：在进气口附近点根蚊香 / 吹口气，看 raw 会不会涨");
  }

  delay(1000);
}
