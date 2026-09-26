/*
 * 继电器测试 —— 4 路逐路点动，验证接线
 * ============================================
 * 目的：**在并进主程序之前，先确认四路继电器都接对了、都好使。**
 *
 * 为什么不直接上主程序：主程序跑起自动逻辑后继电器会随传感器变化乱响，
 * 那时候分不清「响得不对」是接线问题还是逻辑问题。先单独测一轮最省事。
 *
 * ── 接线（⚠️ 跳线帽必须拔掉）────────────────────
 *   模块         接到
 *   JD-VCC  ───→  5V         只给线圈供电
 *   VCC     ───→  3.3V       只给光耦输入侧供电
 *   GND     ───→  GND        ⚠️ 必须和 ESP32 共地
 *   IN1..4  ───→  GPIO 25 / 26 / 33 / 13
 *
 *   拔跳线的原因见 relay.h 头部注释（简单说：不拔会有 1.7V 压差，
 *   光耦可能微导通，继电器抖动发热）。
 *
 * ── 它会做什么 ────────────────────────────────
 *   循环三轮，每轮：
 *     ① 通道 1 吸合 1 秒 → 释放 1 秒
 *     ② 通道 2 同上，③ ④ 依次
 *     ③ 四路**同时**吸合 1 秒 → 再全释放
 *         （这一步顺便测供电够不够 —— 4 路一起吸合约 280mA）
 *
 * ── 怎么判断 ──────────────────────────────────
 *   · 听到「咔哒」声、模块上对应通道的 LED 亮 → 那一路通的 ✓
 *   · 某一路没反应        → 那路的 IN 线没接好 / 引脚不对
 *   · 某个通道一直吸着不放 → 接线接到了别的 GPIO，或者该路被别的东西拉低了
 *   · 四路一起吸合时板子重启/串口断 → **供电不够**，换 DC-DC 的 5V，别用 USB
 *
 * ── 关于串口打印 ──────────────────────────────
 *   每次都把「通道几 / 哪个 GPIO / 现在什么状态」打出来，
 *   方便你对着接线图核对，不用猜。
 */

#include <Arduino.h>
#include "relay.h"

// 每一步之间停多久（毫秒）
const unsigned long STEP_MS = 1000;

void setup() {
  Serial.begin(115200);
  relayInit();          // 上电先把四路都置为「释放」（见 relay.h 里的顺序说明）

  Serial.println();
  Serial.println("=== 4 路继电器测试 ===");
  Serial.println("  接线：JD-VCC→5V  VCC→3.3V  GND→GND  ⚠️ 跳线帽必须拔掉");
  for (int i = 0; i < RELAY_COUNT; i++) {
    Serial.print("  通道 ");
    Serial.print(i + 1);
    Serial.print(" = ");
    Serial.print(RELAY_NAME[i]);
    Serial.print("  ←→  GPIO");
    Serial.println(RELAY_PIN[i]);
  }
  Serial.println();
  Serial.println("  预期：每一路依次「咔哒」响一声，模块上对应 LED 亮 1 秒");
  Serial.println("  某路没声音 → 那路的 IN 线没接好 / 引脚不对");
  Serial.println();
  delay(1000);
}

// 让某一路吸合/释放，并把状态打出来
void step(int ch, bool on) {
  relaySet(ch, on);
  Serial.print("  通道 ");
  Serial.print(ch + 1);
  Serial.print(" (");
  Serial.print(RELAY_NAME[ch]);
  Serial.print(", GPIO");
  Serial.print(RELAY_PIN[ch]);
  Serial.print(") → ");
  Serial.print(on ? "吸合" : "释放");
  // 顺便读回真实引脚状态，确认写下去确实生效了
  Serial.print("   [读回=");
  Serial.print(relayGet(ch) ? "吸合" : "释放");
  Serial.println("]");
  delay(STEP_MS);
}

void loop() {
  Serial.println("── 逐路点动 ──────────────────────────");
  for (int ch = 0; ch < RELAY_COUNT; ch++) {
    step(ch, true);      // 吸合 1 秒
    step(ch, false);     // 释放 1 秒
  }

  // 四路一起吸合 —— 这一步是在测供电：4 路约 280mA，
  // 如果这时候板子重启、串口断掉，就是 5V 供电不够
  Serial.println("── 四路同时吸合（测供电）──────────────");
  for (int ch = 0; ch < RELAY_COUNT; ch++) relaySet(ch, true);
  Serial.print("  四路全吸合，等 1 秒…  当前状态：");
  for (int ch = 0; ch < RELAY_COUNT; ch++) {
    Serial.print(relayGet(ch) ? "吸" : "放");
  }
  Serial.println("  （应该全是「吸」）");
  delay(STEP_MS);

  for (int ch = 0; ch < RELAY_COUNT; ch++) relaySet(ch, false);
  Serial.println("  全部释放");
  Serial.println();
  delay(STEP_MS);
}
