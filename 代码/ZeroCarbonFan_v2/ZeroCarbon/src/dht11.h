#pragma once
/*
 * 温湿度传感器模块 —— DHT11（YL-47 模块，3 线）
 * ============================================
 * 和 light.h / pm25.h 一个套路：常量 + dht11Init() + dht11Read()。
 *
 * ── 接线 ──────────────────────────────────────
 *   YL-47 模块        ESP32
 *   VCC  (1 脚)  ───→  3.3V        ⚠️ 不是 5V
 *   DATA (2 脚)  ───→  GPIO27
 *   GND  (3 脚)  ───→  GND
 *
 *   **上拉不用自己加** —— YL-47 板上已有 R1 4.7k（手册建议 5k，符合）。
 *   模块上还有电源指示灯 D1，**D1 亮 = 模块通电**。
 *
 *   为什么接 3.3V：手册写 3~5.5V 都行，但 DATA 靠**上拉到 VCC**，
 *   接 5V 则 DATA 高电平 = 5V，而 ESP32 的 GPIO 不是 5V 容限。
 *   （和光敏模块同理；和 PM2.5 转接板相反 —— 那块必须 5V。）
 *
 * ── 引脚选择：GPIO27 ──────────────────────────
 *   DHT 是**单总线数字信号**，不挑 ADC，普通 GPIO 就行。
 *   GPIO27 空闲、非 strapping（0/2/5/12/15）、非 Flash（6~11）、非串口（1/3）。
 *   （它虽是 ADC2/Touch7，但这里用的是数字信号、不碰 ADC，
 *     所以「ADC2 与 WiFi 冲突」那条限制不适用。）
 *
 * ── 三条硬约束（手册要求，不遵守读出来就是乱的）──
 *   ① **上电后必须等 1 秒**才能发指令 —— 手册原文：
 *      「传感器上电后，要等待 1s 以越过不稳定状态，在此期间无需发送任何指令」
 *      dht11Init() 里已经 delay(1000)。
 *   ② **采样周期 ≥ 1 秒** —— DHT11 最快约 1 秒一次。本项目用 2 秒。
 *   ③ **本身就是会失败的** —— DHT11 时序敏感，偶发读失败是正常的，
 *      不是"接错了"。所以 dht11Read() 失败时**不要清零显示**，
 *      保留上一次成功的值（见 main.cpp 的 loop）。
 *
 * ── ⚠️ 开 WiFi 之后成功率会下降（重要）────────────────
 *   DHT11 靠**微秒级时序**区分 0 和 1（低电平结束后 35µs 采样）。
 *   主程序开着 WiFi 热点，**WiFi 中断会打断这个时序**，导致读失败。
 *
 *   这是 DHT11 + ESP32 + WiFi 的**固有问题**，不是接线问题。
 *   应对办法（本项目已采用）：
 *     · 失败自动重试一次
 *     · 失败时**保留上一次成功的读数**，网页不会跳成 0
 *     · 采样周期放宽到 2 秒
 *   如果将来发现成功率太低，可以考虑：
 *     · 换成 DHT22（时序容差大一些）
 *     · 或改用 I2C 的 SHT30 / AHT20（不靠微秒时序，稳得多）
 *
 * ── 关于为什么不用库 ──────────────────────────
 *   常见的 DHT 库（Adafruit DHT / DHTesp）要往 platformio.ini 加 lib_deps。
 *   这里手写位操作，**不用装任何库**。
 *
 * ── DHT11 规格（厂商手册）─────────────────────
 *   湿度 20–90 %RH（±5 %RH）｜ 温度 0–50 °C（±2 °C）｜ **分辨力 1**
 *   注意分辨力是 1：温度和湿度都是**整数，没有小数位** —— 这是正常的，
 *   也是 DHT11 和 DHT22 的主要差别。
 */

#include <Arduino.h>

// ================== 常量：本模块全部参数都在这 ==================
const int DHT_PIN = 27;              // 单总线数据脚

const unsigned long DHT_READ_MS = 2000;   // 采样周期：2 秒（手册要求 ≥1 秒）
const int DHT_START_LOW_MS   = 20;        // 起始信号拉低 ≥18ms（取 20ms 留余量）
const int DHT_BIT_SAMPLE_US  = 35;        // 每位在高电平期间采样：35µs 时还高 = 1
const int DHT_WAIT_TIMEOUT_US = 200;      // 等电平翻转的超时，防死等

// ================== 初始化：setup() 里调一次 ==================
inline void dht11Init() {
  pinMode(DHT_PIN, INPUT);      // 空闲时靠模块上的 4.7k 上拉保持高电平
  // 手册明文要求：上电后等 1s 越过不稳定状态
  delay(1000);
}

// ================== 内部：等引脚变成指定电平 ==================
// 带超时，避免信号异常时 while 死循环卡住整个程序
inline bool dhtWaitLevel(int level, unsigned long timeoutUs) {
  unsigned long t0 = micros();
  while (digitalRead(DHT_PIN) != level) {
    if (micros() - t0 > timeoutUs) return false;
  }
  return true;
}

// ================== 读取：loop() 里定时调 ==================
// 成功返回 true，并把整数温湿度写进 tempOut / humiOut。
// 失败返回 false，**两个输出参数不动** —— 调用方应保留上一次的值。
//
// DHT11 通信过程（单总线，一根线双向）：
//   ① 主机拉低 ≥18ms 再放开              —— 告诉它「我要读数据了」
//   ② DHT11 应答：拉低 80µs，再拉高 80µs  —— 「我准备好了」
//   ③ DHT11 连发 40 位（5 字节）：湿整 湿小 温整 温小 校验和
//      每位 = 「低 50µs + 高 X µs」，X≈26µs 是 0，X≈70µs 是 1
//      所以在「低电平结束后 35µs」读引脚：还高 = 1，已低 = 0
inline bool dht11Read(int8_t& tempOut, int8_t& humiOut) {
  uint8_t data[5] = {0};

  // ① 起始信号
  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, LOW);
  delay(DHT_START_LOW_MS);
  pinMode(DHT_PIN, INPUT);        // 放开，靠上拉拉高

  // ② 等应答：先低 80µs，再高 80µs
  if (!dhtWaitLevel(LOW,  DHT_WAIT_TIMEOUT_US)) return false;
  if (!dhtWaitLevel(HIGH, DHT_WAIT_TIMEOUT_US)) return false;

  // ③ 读 40 位
  for (int i = 0; i < 40; i++) {
    if (!dhtWaitLevel(LOW,  DHT_WAIT_TIMEOUT_US)) return false;  // 每位的低电平
    if (!dhtWaitLevel(HIGH, DHT_WAIT_TIMEOUT_US)) return false;  // 等它结束
    delayMicroseconds(DHT_BIT_SAMPLE_US);                        // 35µs 后采样
    data[i >> 3] <<= 1;
    if (digitalRead(DHT_PIN) == HIGH) data[i >> 3] |= 1;
  }

  // ④ 校验：第 5 字节 = 前 4 字节之和的低 8 位
  if (data[4] != (uint8_t)(data[0] + data[1] + data[2] + data[3])) return false;

  // DHT11：data[0]=湿度整数 data[1]=湿度小数(恒0)
  //        data[2]=温度整数 data[3]=温度小数(恒0)
  humiOut = (int8_t)data[0];
  tempOut = (int8_t)data[2];
  return true;
}
