/*
 * 点灯测试 —— 交替闪烁
 * ============================================
 * 目的：烧录后确认两件事
 *   1. 板子跑得起来你写的代码  → 看串口
 *   2. LED 引脚选得对不对       → 看灯
 *
 * 怎么看结果（串口监视器，波特率 115200）：
 *   每 500ms 打一行，形如
 *       [1500] LED = ON   亮
 *       [2000] LED = OFF  灭
 *
 *   · 串口在刷 + 灯在闪  → 全对，板子没问题
 *   · 串口在刷 + 灯不闪  → 代码没问题，只是下面的 LED_PIN 选错了，换一个再烧
 *   · 串口没输出         → 是烧录 / CP2102 驱动 / 波特率的问题，跟这段代码无关
 *
 * 关于 delay()：
 *   这里用 delay() 是安全的。delay() 只在 WebServer 那种要频繁
 *   handleClient() 的程序里才是坑（会把网页卡死），本程序没有任何
 *   网络要服务，阻塞等待反而是最简单、最好读懂的写法。
 */

#include <Arduino.h>

// ==================== 需要你核对的地方 ====================
//
// 板卡引脚图（文档/板卡/esp32_cp2102_引脚图.jpg）上 38 个脚没有一个标 LED，
// 实物照片也认不出可控 LED —— 所以下面这个值是**待确认**的，不是查出来的。
//
// 为什么先填 2：多数 ESP32 开发板把用户 LED 挂在 GPIO2（这样上电时
// 可以用它提示启动状态）。但你这块 WAVGAT 板在任何图上都没标注，
// 这属于「常见做法」，**不是「已确认事实」**。
//
// 换引脚时避开这些：
//   GPIO 6~11   接着板载 Flash，碰了起不来也烧不进去
//   GPIO 34/35/36/39  只能输入，输出不了
//   GPIO 1(TX0)/3(RX0)  USB 串口在用，占了看不到输出
//   安全可用：25 / 26 / 27 / 32 / 33
const int LED_PIN = 2;

// 亮多久、灭多久（毫秒）。500ms = 每秒完整闪一次，肉眼看着最舒服。
const unsigned long BLINK_MS = 500;

// ==================== 初始化 ====================
void setup() {
  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_PIN, LOW);   // 先灭掉，免得上电瞬间状态不确定

  // 115200 必须和 platformio.ini 里的 monitor_speed 一致，
  // 不一致的话监视器看到的是一片乱码。
  Serial.begin(115200);

  Serial.println();
  Serial.println("=== 点灯测试启动 ===");
  Serial.print("LED 引脚 = GPIO");
  Serial.println(LED_PIN);
  Serial.println("预期：灯每 500ms 亮灭交替一次");
  Serial.println();
}

// ==================== 主循环 ====================
void loop() {
  digitalWrite(LED_PIN, HIGH);
  Serial.print("[");
  Serial.print(millis());
  Serial.println("] LED = ON   亮");
  delay(BLINK_MS);

  digitalWrite(LED_PIN, LOW);
  Serial.print("[");
  Serial.print(millis());
  Serial.println("] LED = OFF  灭");
  delay(BLINK_MS);
}
