/*
 * 光照传感器测试（光敏电阻传感器模块 · **4 线制**）
 * ============================================
 * 独立测试程序：只读 AO/DO、往串口打印，不开 WiFi。
 * 用来确认「接线对不对、模块活没活、AO 量程、DO 极性」。
 *
 * ── 4 线制接口（依据 文档/传感器/光敏传感器4线/光敏电阻4线-原理图.jpg） ──
 *   ① VCC   ② GND   ③ DO   ④ AO
 *   （同图还画了 3 线制：① VCC ② GND ③ DO —— 少一根 AO）
 *   ⚠️ 接线前先核对板子上的**丝印**，以实物为准。
 *
 * 内部电路：
 *   VCC ──[10K]──┬── 节点 ──[光敏电阻]── GND      （并联 104 电容）
 *                 ├── AO（模拟量，直接引出）
 *                 └── LM393 「+」/「−」之一
 *   VR1 10K 电位器（蓝色）── LM393 另一个输入，调 DO 的翻转阈值
 *   LM393 输出 ──┬──[10K]── VCC（上拉）
 *                ├──[1K]── 开关指示 LED ── VCC
 *                └── DO
 *
 * ── 两条结论的把握程度不一样，别混 ──────────────
 *   ✅ **AO：高 = 暗，低 = 亮**（确定）
 *      由分压拓扑直接决定：天暗 → 光敏电阻阻值↑ → 节点电压↑。
 *      不依赖 LM393 的同相/反相接法，怎么接都成立。
 *   ⚠️ **DO：低 = 暗**（强证据，但请实测确认）
 *      依据 参考例程.txt（GBK，已解码）：
 *          if (val == LOW)  // 当光敏电阻传感器检测有信号时，LED 亮
 *              digitalWrite(Led, HIGH);
 *      夜灯语义下「检测有信号」= 天暗。若实测相反，改下面的 DARK_LEVEL。
 *
 * ── 接线 ──────────────────────────────────────
 *   模块 VCC → ESP32 **3.3V**   ← 不是 5V！DO 有 10K 上拉到 VCC，
 *                                 接 5V 则 DO 高电平 = 5V，ESP32 GPIO 不是 5V 容限。
 *                                 LM393 工作电压 2~36V，3.3V 完全正常。
 *   模块 GND → ESP32 GND
 *   模块 DO  → ESP32 GPIO32     （数字）
 *   模块 AO  → ESP32 GPIO35     （模拟；34 已被 PM2.5 占用）
 *
 * ── 怎么读串口 ────────────────────────────────
 *   AO=1234  40%   DO=低 → 暗
 *   · AO 范围 0~4095（3.3V 满量程）。**越暗数值越大**
 *   · 每秒一行，AO 跨越阈值或 DO 变化时多打一行「== 变化 ==」
 *
 *   判定流程：
 *   1. 拧板上蓝色电位器，让「开关指示」灯刚好在明暗之间跳变
 *   2. **用手完全遮住传感器**，看 AO 是不是明显变大、DO 有没有翻转
 *   3. 若 AO 趋势反了 → 说明分压接反，查接线
 *      若 AO 对但暗/亮标反了 → 改 DARK_LEVEL
 */

#include <Arduino.h>

// ================== 引脚 ==================
// GPIO32：空闲、非 strapping（0/2/5/12/15）、非 Flash（6~11）、非 USB 串口（1/3）
// GPIO35：只读输入脚（无输出能力），正适合做 ADC，且 34 已被 PM2.5 模拟输入占用
const int LIGHT_DO_PIN = 32;
const int LIGHT_AO_PIN = 35;

// ================== 极性 / 阈值 ==================
// DO 什么电平代表「暗」—— 见文件头：厂商例程说 LOW，但请实测确认。
const int DARK_LEVEL = LOW;          // 约定：DO=低 表示暗；实测相反就改成 HIGH

// AO 判定阈值：**高于它算暗**（AO 高 = 暗，拓扑决定）。
// 0~4095，取中间值起步，实测后再调。
const int DARK_AO_THRESHOLD = 2048;

// 用 INPUT_PULLUP：模块没接/没上电时引脚不悬空，避免读数乱跳被误判成线接错。
// 上拉约 45kΩ，比模块板上那颗 10k 弱得多 —— 模块正常供电时它说了算，不干扰。
// 代价：模块没接时 DO 会稳定显示「暗」，这是预期不是 bug。

void setup() {
  // 115200 必须和 platformio.ini 的 monitor_speed 一致，否则是乱码
  Serial.begin(115200);
  pinMode(LIGHT_DO_PIN, INPUT_PULLUP);
  pinMode(LIGHT_AO_PIN, INPUT);      // ADC 输入，不需要也不该开上拉

  Serial.println();
  Serial.println("=== 光照传感器测试（4 线制：VCC/GND/DO/AO）===");
  Serial.print("  DO = GPIO");
  Serial.print(LIGHT_DO_PIN);
  Serial.print("    AO = GPIO");
  Serial.println(LIGHT_AO_PIN);
  Serial.println("  接线：VCC→3.3V（不是5V）  GND→GND  DO→GPIO32  AO→GPIO35");
  Serial.println();
  Serial.println("  当前约定：AO 高=暗（拓扑确定） ｜ DO 低=暗（待实测）");
  Serial.println("  阈值：AO > " + String(DARK_AO_THRESHOLD) + " 判为暗");
  Serial.println();
  Serial.println("  >>> 现在用手完全遮住传感器，AO 应明显变大 <<<");
  Serial.println();
}

// ================== 主循环 ==================
// 用 delay() 没问题 —— 本程序没有任何网络要服务。
void loop() {
  static int lastAo = -1;
  static int lastDo = -1;

  int aoRaw = analogRead(LIGHT_AO_PIN);      // 0 ~ 4095
  int doRaw = digitalRead(LIGHT_DO_PIN);      // HIGH / LOW

  // AO → 百分比（100 = 最亮，0 = 最暗）。AO 高 = 暗，所以取反。
  int lightPct = 100 - (aoRaw * 100L) / 4095;
  if (lightPct < 0)   lightPct = 0;
  if (lightPct > 100) lightPct = 100;

  bool aoDark   = (aoRaw > DARK_AO_THRESHOLD);
  bool doDark   = (doRaw == DARK_LEVEL);
  bool doMapped = (doRaw == LOW);

  // 状态变了才多打一行，方便回看「什么时候变的」
  if (aoRaw != lastAo || doRaw != lastDo) {
    if (lastAo != -1) Serial.println("== 变化 ==");
    lastAo = aoRaw;
    lastDo = doRaw;
  }

  Serial.print("AO=");
  Serial.print(aoRaw);
  Serial.print("(");
  Serial.print(lightPct);
  Serial.print("%亮)  ");
  Serial.print(aoDark ? "AO判暗" : "AO判亮");

  Serial.print("   DO=");
  Serial.print(doRaw ? "高" : "低");
  Serial.print(" → ");
  Serial.print(doMapped ? "暗" : "亮");

  // AO 和 DO 打架 = 极性常量或电位器阈值要调
  if (aoDark != doMapped) {
    Serial.print("   ⚠ 两者矛盾 —— 若 AO 对，改 DARK_LEVEL");
  }
  Serial.println();

  delay(500);
}
