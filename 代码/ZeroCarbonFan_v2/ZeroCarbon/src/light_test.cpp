/*
 * 光照传感器测试（光敏电阻传感器模块 · 3 线制）
 * ============================================
 * 独立测试程序：只读 DO、往串口打印，不开 WiFi。
 * 用来确认「接线对不对、模块活没活、天黑时 DO 到底是高还是低」。
 *
 * ── 模块是什么（依据 文档/传感器/光敏电阻传感器模块3线制/ 的电路图） ──
 *   3 线制接口：① VCC  ② GND  ③ DO
 *   · LM393 比较器 + 光敏电阻分压 + 板上 VR1 10K 电位器（拧它调阈值）
 *   · DO 带 10K 上拉到 VCC —— 所以 DO 高电平 = 模块供电电压
 *   · 板上还有一颗「开关指示」LED，就接在 DO 上
 *   · 3 线制**没把 AO 引出来**，只能判亮/暗两态，读不到具体光照强度
 *     （要 AO 得用 4 线制的）
 *
 * ── 接线（⚠️ 重点看这条） ──────────────────────
 *   模块 VCC → ESP32 **3.3V**   ← 不是 5V！
 *   模块 GND → ESP32 GND
 *   模块 DO  → ESP32 GPIO32
 *
 *   为什么必须 3.3V：DO 有 10K 上拉到 VCC，接 5V 时 DO 高电平就是 5V，
 *   而 ESP32 的 GPIO **不是 5V 容限**，长期接有打坏引脚的风险。
 *   LM393 工作电压 2~36V，用 3.3V 供电完全正常。
 *
 * ── 怎么读串口输出 ────────────────────────────
 *   DO=低 → 判定「暗」    DO=高 → 判定「亮」
 *   每秒一行，状态变化时多打一行「== 变化 ==」
 *
 *   判定流程：
 *   1. 上电，拧板上蓝色电位器，让「开关指示」灯刚好在明暗之间跳变
 *   2. 看串口：**用手完全遮住传感器**，记下 DO 是高还是低
 *   3. 如果串口判的「暗/亮」和实际情况相反 —— 下面的 DARK_LEVEL 改掉即可
 */

#include <Arduino.h>

// ================== 引脚 ==================
// 选 GPIO32 的理由：空闲、不是 strapping 脚（0/2/5/12/15）、
// 不接 Flash（6~11）、不是 USB 串口（1/3），而且物理上挨着
// PM2.5 用的 GPIO34，面包板上好走线。
// 现有占用：GPIO13（PM2.5 LED）、GPIO34（PM2.5 模拟输入）。
const int LIGHT_PIN = 32;

// ================== 极性（待实测，不对就改这里） ==================
// DO 什么电平代表「暗」？电路图上 LM393 的同相/反相端接法看不清，
// 所以不猜，做成常量交给实测。
const int DARK_LEVEL = LOW;   // 约定：DO=低 表示暗。若实测相反改成 HIGH

// ================== 输入模式 ==================
// 用 INPUT_PULLUP 而不是 INPUT：模块没接/没上电时引脚会悬空，
// 悬空读数会乱跳，第一次测试很容易误判成「线接错了」。
// 上拉约 45kΩ，比模块板上那颗 10k 弱得多 —— 模块一旦正常供电，
// 它说了算，上拉不会干扰。
// 代价：模块没接时串口会稳定显示「暗」，这是**预期**，不是 bug。

void setup() {
  // 115200 必须和 platformio.ini 的 monitor_speed 一致，否则是乱码
  Serial.begin(115200);
  pinMode(LIGHT_PIN, INPUT_PULLUP);

  Serial.println();
  Serial.println("=== 光照传感器测试（3 线制 · 只有 DO）===");
  Serial.print("  DO 引脚 = GPIO");
  Serial.println(LIGHT_PIN);
  Serial.println("  接线：VCC→3.3V（不是5V）  GND→GND  DO→GPIO32");
  Serial.println("  当前约定：DO=低 → 暗；DO=高 → 亮");
  Serial.println();
  Serial.println("  >>> 现在用手完全遮住传感器，看下面 DO 会怎么变 <<<");
  Serial.println();
}

// ================== 主循环 ==================
// 用 delay() 没问题 —— 本程序没有任何网络要服务，
// delay() 只在 WebServer 那种要频繁 handleClient() 的场景才是坑。
void loop() {
  static int lastRaw = -1;
  static bool lastDark = false;

  int raw = digitalRead(LIGHT_PIN);   // 高 / 低
  bool dark = (raw == DARK_LEVEL);    // 按约定换算成 暗/亮

  // 只在状态变化时多打一行，方便回看「什么时候变的」
  if (raw != lastRaw) {
    if (lastRaw != -1) Serial.println("== 变化 ==");
    lastRaw = raw;
    lastDark = dark;
  }

  Serial.print("DO=");
  Serial.print(raw ? "高" : "低");
  Serial.print("  →  ");
  Serial.print(dark ? "暗" : "亮");
  if (dark != lastDark) {
    Serial.print("   ⚠ 判定方向和之前相反 —— 若与实际不符，改 DARK_LEVEL");
    lastDark = dark;
  }
  Serial.println();

  delay(500);
}
