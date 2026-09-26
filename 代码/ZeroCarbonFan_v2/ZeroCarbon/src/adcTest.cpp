/*
 * ADC 单独测试 —— 验证 ESP32 模拟输入这条路本身好不好
 * ============================================
 * 为什么需要这个：PM2.5 调试时发现，GPIO32 短接到稳定的 3.3V，
 * 读数却在 112~2168 之间乱跳、校准后只有约 1000mV（应该 4095 / 3300mV）。
 * 也就是说**读数这条路本身有问题**，之前基于它做的判断全不可信。
 *
 * 这个程序把传感器、滤波、采样时序**全部拿掉**，只读 ADC，
 * 用来分辨到底是「引脚坏了」「ADC 配置不对」还是「两种读法互相干扰」。
 *
 * ── 怎么用 ────────────────────────────────────
 * 1. 把要测的脚**直接短接到 3.3V**（或 GND），别的什么线都别接
 * 2. 烧录 [env:adctest]，开串口 115200
 * 3. 对照下表看输出
 *
 * ── 三个关键实验（程序会自动轮流做）────────────
 *   实验 A：连读 10 次 analogRead()          → 3.3V 时应全部是 4095
 *   实验 B：连读 10 次 analogReadMilliVolts() → 3.3V 时应全部是 ~3300
 *   实验 C：交替调用两者                      → 看会不会互相干扰
 *
 * ── 怎么判 ────────────────────────────────────
 *   A 稳、B 稳、C 也稳        → ADC 没问题，之前的乱跳是别的代码引起的
 *   A 稳、B 乱               → analogReadMilliVolts() 本身有问题
 *   A 乱、B 乱               → 引脚或 ADC 硬件有问题 → 换个脚（33/35/36/39）再测
 *   A、B 都稳但 C 乱          → **两种读法互相干扰** → 代码里只能留一个
 */

#include <Arduino.h>

// 要测的脚 —— 短接到 3.3V 或 GND。想换脚改这里（必须是 ADC1：32/33/34/35/36/39）
const int TEST_PIN = 32;

const int N = 10;          // 每轮连读几次
const int GAP_MS = 30;

void setup() {
  Serial.begin(115200);
  pinMode(TEST_PIN, INPUT);
  Serial.println();
  Serial.print("=== ADC 单独测试 ===  引脚 = GPIO");
  Serial.println(TEST_PIN);
  Serial.println("  请把这个脚短接到 3.3V 或 GND，别的线都不要接");
  Serial.println("  预期：接 3.3V → analogRead 稳定 4095、mV 稳定 ~3300");
  Serial.println("        接 GND  → 两者都稳定 0");
  Serial.println();
}

void loop() {
  // ── 实验 A：只用 analogRead，连读 10 次 ──
  Serial.print("[A] analogRead      :");
  for (int i = 0; i < N; i++) {
    Serial.print(" ");
    Serial.print(analogRead(TEST_PIN));
    delay(GAP_MS);
  }
  Serial.println();

  // ── 实验 B：只用 analogReadMilliVolts，连读 10 次 ──
  Serial.print("[B] milliVolts      :");
  for (int i = 0; i < N; i++) {
    Serial.print(" ");
    Serial.print(analogReadMilliVolts(TEST_PIN));
    delay(GAP_MS);
  }
  Serial.println();

  // ── 实验 C：交替调用，看两者会不会互相干扰 ──
  Serial.print("[C] 交替(读数/毫伏) :");
  for (int i = 0; i < N; i++) {
    Serial.print(" ");
    Serial.print(analogRead(TEST_PIN));
    Serial.print("/");
    Serial.print(analogReadMilliVolts(TEST_PIN));
    delay(GAP_MS);
  }
  Serial.println();

  Serial.println("  ── A稳+B稳+C也稳 = ADC 没问题；A稳B乱 = 校准读法有问题；");
  Serial.println("     A乱B乱 = 引脚/ADC 硬件问题，换个脚试；A、B稳但C乱 = 两种读法互相干扰");
  Serial.println();
  delay(2000);
}
