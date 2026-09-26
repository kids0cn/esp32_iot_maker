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

// 实验 E 用：这个脚就是传感器程序里的 ILED 脚，用来复现同样的时序干扰
const int LED_PIN = 14;
const int SAMPLING_US   = 280;
const int SLEEP_US      = 9680;

void setup() {
  Serial.begin(115200);
  pinMode(TEST_PIN, INPUT);
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, HIGH);
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

  // ── 实验 C：交替调用，看两者会不会互相干扰（中间夹了打印，约 350µs 空隙）──
  Serial.print("[C] 交替(读数/毫伏) :");
  for (int i = 0; i < N; i++) {
    Serial.print(" ");
    Serial.print(analogRead(TEST_PIN));
    Serial.print("/");
    Serial.print(analogReadMilliVolts(TEST_PIN));
    delay(GAP_MS);
  }
  Serial.println();

  // ── 实验 D：两种读法**紧挨着**调用，中间一点空隙都不留 ──
  //   传感器程序里就是这么写的：连读两次、中间没有 Serial.print。
  //   如果这条坏了而 C 是好的，说明「两次 ADC 紧挨着」才是问题。
  Serial.print("[D] 紧挨(读数/毫伏) :");
  for (int i = 0; i < N; i++) {
    int a = analogRead(TEST_PIN);
    int b = (int)analogReadMilliVolts(TEST_PIN);
    Serial.print(" ");
    Serial.print(a);
    Serial.print("/");
    Serial.print(b);
    delay(GAP_MS);
  }
  Serial.println();

  // ── 实验 E：完整复现传感器程序的时序（拉一个脚 → 等 280µs → 紧挨着连读两次）──
  //   用 GPIO14（就是 ILED 那个脚）制造同样的干扰条件。
  //   如果 E 坏了，说明是「拉脚造成的电源扰动 + 紧挨读」；
  //   如果 E 是好的，那问题就出在传感器本身的负载上。
  Serial.print("[E] 复现传感器时序  :");
  for (int i = 0; i < N; i++) {
    digitalWrite(LED_PIN, LOW);          // 拉低（传感器程序里 LED_ON_LEVEL = LOW）
    delayMicroseconds(SAMPLING_US);      // 等 280µs
    int a = analogRead(TEST_PIN);        // 紧挨着连读两次
    int b = (int)analogReadMilliVolts(TEST_PIN);
    digitalWrite(LED_PIN, HIGH);         // 拉回
    delayMicroseconds(SLEEP_US);         // 补足 10ms
    Serial.print(" ");
    Serial.print(a);
    Serial.print("/");
    Serial.print(b);
  }
  Serial.println();

  Serial.println("  ── 判读 ─────────────────────────────");
  Serial.println("   A/B/C/D/E 全稳       → ADC 和时序都没问题，问题在传感器负载");
  Serial.println("   C 稳但 D 乱          → **两次 ADC 紧挨着读**才是元凶，中间要留空隙");
  Serial.println("   D 稳但 E 乱          → **拉 GPIO 造成的电源扰动**才是元凶");
  Serial.println("   A 或 B 单独乱        → 引脚/ADC 硬件问题，换个脚（33/35/36/39）");
  Serial.println();
  delay(2000);
}
