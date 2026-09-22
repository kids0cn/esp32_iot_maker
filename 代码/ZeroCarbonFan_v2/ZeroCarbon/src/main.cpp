/*
 * 零碳新风智能家居系统 —— WiFi 热点 + 手机端网页
 * ============================================
 * 烧录后手机连热点，浏览器打开 http://192.168.4.1 就能看到界面。
 *
 * ── 这版做了什么 ──────────────────────────────
 *   1. ESP32 自己开一个 WiFi 热点（不是连家里的路由器）
 *   2. 起一个网页服务器，把 include/index_html.h 里的页面发出去
 *   3. 实现页面要调的 5 个接口，让按钮、模式切换真的有反应
 *
 * ── 哪些是真的、哪些还是空的（重要，别当真） ──
 *   ✅ WiFi 热点        —— 真的
 *   ✅ 网页服务 + 接口  —— 真的，手机能开、按钮有反应
 *   ⬜ PM2.5 读数       —— **假的，恒为 0**。传感器还没接线，没东西可读
 *   ⬜ 风扇开关         —— **只是 RAM 里的一个变量**。没接继电器，
 *                          也**没有去驱动任何 GPIO** —— 引脚没核实就输出，
 *                          有烧板子的风险，所以一个引脚都不碰
 *
 * ── 为什么 loop() 里不能有 delay() ────────────
 *   WebServer 库靠 server.handleClient() 一轮一轮地收发数据。
 *   一旦在 loop 里 delay()，这段时间网页请求就全被堵住，
 *   手机上表现为「点了没反应 / 转圈 / 连接中断」。
 *   所以这里只 handleClient()，要做定时的事一律用 millis() 比时间。
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include "index_html.h"

// ================== WiFi 热点配置 ==================
// 手机连的就是这两个。改名字改密码只改这里。
// 密码 WPA2 要求至少 8 位，写短了 softAP() 会直接失败、热点起不来。
const char* WIFI_SSID = "ZeroCarbon";   // 热点名（手机 WiFi 列表里看到的）
const char* WIFI_PASS = "12345678";     // 密码

WebServer server(80);

// ================== 运行状态 ==================
// 全部只放在 RAM 里：断电或复位就回到默认值。
// 没有用 NVS/Flash 做持久化 —— 现在是联调阶段，先保持简单。
enum Mode { MODE_AUTO, MODE_MANUAL };
Mode mode = MODE_AUTO;                  // 开机默认自动模式

bool fanOn = false;                     // 风扇状态（只是变量，没接真实继电器）
int pm25 = 0;                           // 恒为 0 —— 传感器还没接

const int PM25_THRESHOLD_ON = 75;       // 超过这个值开风扇
const int PM25_THRESHOLD_OFF = 35;      // 低于这个值关风扇（和上面构成回差，防止频繁启停）

// ================== 统一的 JSON 状态响应 ==================
// 页面每秒来问一次；每个会改状态的接口也用它回话，格式统一好处理。
void sendState() {
  String json = "{";
  json += "\"pm25\":" + String(pm25);
  json += ",\"fan\":" + String(fanOn ? "true" : "false");
  json += ",\"mode\":\"" + String(mode == MODE_AUTO ? "auto" : "manual") + "\"";
  json += ",\"on\":" + String(PM25_THRESHOLD_ON);
  json += ",\"off\":" + String(PM25_THRESHOLD_OFF);
  json += "}";
  server.send(200, "application/json", json);
}

// ================== 路由表 ==================
// 路径必须和页面 JS 里写的完全一致，差一个字符页面就连不上。
void setupServer() {
  server.on("/", []() {
    server.send(200, "text/html", INDEX_HTML);
  });

  server.on("/api/state", sendState);   // 页面每秒轮询这个

  // 风扇：目前只改内存里的变量，不碰任何 GPIO（没核实引脚，不能乱输出）
  server.on("/fan/on", []() {
    fanOn = true;
    sendState();
  });
  server.on("/fan/off", []() {
    fanOn = false;
    sendState();
  });

  // 模式切换
  server.on("/mode/auto", []() {
    mode = MODE_AUTO;
    sendState();
  });
  server.on("/mode/manual", []() {
    mode = MODE_MANUAL;
    sendState();
  });

  // 浏览器会自动来要图标，回个 204，免得串口刷一堆 404
  server.on("/favicon.ico", []() {
    server.send(204, "text/plain", "");
  });

  server.onNotFound([]() {
    server.send(404, "text/plain", "Not Found");
  });

  server.begin();
}

// ================== 初始化 ==================
void setup() {
  Serial.begin(115200);

  // 开热点。softAP 内部默认分配 192.168.4.1，不用额外配。
  WiFi.softAP(WIFI_SSID, WIFI_PASS);

  setupServer();

  Serial.println();
  Serial.println("零碳新风智能家居系统 v3");
  Serial.println("热点已启动");
  Serial.print("  热点名：");
  Serial.println(WIFI_SSID);
  Serial.print("  密码：");
  Serial.println(WIFI_PASS);
  Serial.print("  手机连上后访问：http://");
  Serial.println(WiFi.softAPIP());
  Serial.println();
}

// ================== 主循环 ==================
void loop() {
  server.handleClient();   // 必须频繁调用，中间不能塞 delay()
}
