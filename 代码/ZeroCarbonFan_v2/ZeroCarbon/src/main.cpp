/*
 * 零碳新风智能家居系统 —— WiFi 热点 + 手机端网页
 * ============================================
 * 烧录后手机连热点，浏览器打开 http://192.168.4.1 就能看到界面。
 *
 * ── 这版做了什么 ──────────────────────────────
 *   1. ESP32 自己开一个 WiFi 热点（不是连家里的路由器）
 *   2. 起一个网页服务器，把 include/index_html.h 里的页面发出去
 *   3. 实现页面要调的 5 个接口，让按钮、模式切换真的有反应
 *   4. 强制门户（Captive Portal）：手机一连上就自动弹出控制页
 *      —— 就是酒店/机场 WiFi 登录弹窗那套机制
 *
 * ── 哪些是真的、哪些还是空的（重要，别当真） ──
 *   ✅ WiFi 热点        —— 真的
 *   ✅ 网页服务 + 接口  —— 真的，手机能开、按钮有反应
 *   ⬜ PM2.5 读数       —— **假的，恒为 0**。传感器还没接线，没东西可读
 *   ⬜ 风扇开关         —— **只是 RAM 里的一个变量**。没接继电器
 *   ⬜ 光照             —— 读 GPIO32 的 DO（**只读输入，不输出电平**）。
 *                          模块没接时 INPUT_PULLUP 会稳定显示「暗」，属预期。
 *                          仍然**没有驱动任何输出引脚** —— 引脚没核实就输出
 *                          有烧板子的风险。
 *
 * ── 为什么 loop() 里不能有 delay() ────────────
 *   WebServer 库靠 server.handleClient() 一轮一轮地收发数据。
 *   一旦在 loop 里 delay()，这段时间网页请求就全被堵住，
 *   手机上表现为「点了没反应 / 转圈 / 连接中断」。
 *   所以这里只 handleClient()，要做定时的事一律用 millis() 比时间。
 *   DNS 服务同理 —— dnsServer.processNextRequest() 也必须频繁调用。
 *
 * ── 弹窗能做到什么程度（实测才知道，别抱太大期望） ──
 *   iPhone / 原生 Android：基本都会自动弹。
 *   国产 ROM（MIUI / ColorOS / EMUI / 鸿蒙）：经常不弹，或只弹一次，
 *     多数要手动点一下已连上的 WiFi 名称才出登录页。
 *   所有手机都会提示「已连接，无互联网」—— 没接上游网络，正常。
 *   弹出来的是系统内置小窗浏览器，地址栏被锁；真正操作还是建议
 *   手动开 http://192.168.4.1 体验更好。
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "index_html.h"

// ================== WiFi 热点配置 ==================
// 手机连的就是这两个。改名字改密码只改这里。
// 密码 WPA2 要求至少 8 位，写短了 softAP() 会直接失败、热点起不来。
const char* WIFI_SSID = "ZeroCarbon";   // 热点名（手机 WiFi 列表里看到的）
const char* WIFI_PASS = "12345678";     // 密码

WebServer server(80);

// ================== 通配 DNS（强制门户的关键） ==================
// 手机连上热点后，会主动去请求公网上的探测地址来判断「要不要登录」，
// 例如 Android 的 connectivitycheck.gstatic.com/generate_204。
// 这些域名本来就解析不到你这儿 —— 不开 DNS 的话，探测请求根本到不了
// 板子，系统只会说「无互联网」，永远不会弹窗。
//
// 通配 DNS 的作用：**把任何域名都答成 192.168.4.1**，
// 于是探测请求就被骗到本机，由我们的路由接管。
// "*" 是通配，53 是 DNS 标准端口。DNSServer 是 ESP32 核心自带的，不用装库。
const byte DNS_PORT = 53;
DNSServer dnsServer;

// ================== 运行状态 ==================
// 全部只放在 RAM 里：断电或复位就回到默认值。
// 没有用 NVS/Flash 做持久化 —— 现在是联调阶段，先保持简单。
enum Mode { MODE_AUTO, MODE_MANUAL };
Mode mode = MODE_AUTO;                  // 开机默认自动模式

bool fanOn = false;                     // 风扇状态（只是变量，没接真实继电器）
int pm25 = 0;                           // 恒为 0 —— 传感器还没接

const int PM25_THRESHOLD_ON = 75;       // 超过这个值开风扇
const int PM25_THRESHOLD_OFF = 35;      // 低于这个值关风扇（和上面构成回差，防止频繁启停）

// ================== 光照传感器（3 线制光敏模块）==================
// 接线：模块 VCC → **3.3V**（不是 5V！DO 有 10K 上拉到 VCC，
//              接 5V 的话 DO 高电平就是 5V，ESP32 GPIO 不是 5V 容限）
//       模块 GND → GND     模块 DO → GPIO32
// 选 GPIO32：空闲、非 strapping（0/2/5/12/15）、非 Flash（6~11）、
//            非 USB 串口（1/3），且物理上挨着 PM2.5 的 GPIO34，好走线。
const int LIGHT_PIN = 32;
// DO 什么电平代表「暗」—— 电路图上 LM393 的相位看不清，不猜，做成常量交给实测。
// 实测方法见 src/light_test.cpp（env:light）。
const int DARK_LEVEL = LOW;

// ================== 统一的 JSON 状态响应 ==================
// 页面每秒来问一次；每个会改状态的接口也用它回话，格式统一好处理。
void sendState() {
  String json = "{";
  json += "\"pm25\":" + String(pm25);
  json += ",\"fan\":" + String(fanOn ? "true" : "false");
  json += ",\"mode\":\"" + String(mode == MODE_AUTO ? "auto" : "manual") + "\"";
  json += ",\"light\":\"" + String(digitalRead(LIGHT_PIN) == DARK_LEVEL ? "dark" : "bright") + "\"";
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

  // ===== 强制门户：凡是没匹配上的路径，一律 302 跳回首页 =====
  //
  // 手机的连通性探测全部落到这里。各系统要的路径不一样：
  //   Android  /generate_204           期望 HTTP 204 空响应
  //   iOS      /hotspot-detect.html    期望一个特定的成功页
  //   Windows  /connecttest.txt /ncsi.txt
  //   Firefox  /success.txt /canonical.html
  // 我们**故意不按预期回答**，改成 302 重定向 —— 系统就判定
  // 「这是需要登录的网络」，于是自动弹出内置浏览器加载首页。
  //
  // 用 onNotFound 兜底而不是逐个列举：探测地址会随系统版本变，
  // 兜底能覆盖所有变体，也不用以后每出一个新地址就改一次代码。
  server.onNotFound([]() {
    server.sendHeader("Location",
                      String("http://") + WiFi.softAPIP().toString() + "/",
                      true);
    server.send(302, "text/plain", "");
  });

  server.begin();
}

// ================== 初始化 ==================
void setup() {
  Serial.begin(115200);

  // 光照 DO 设为输入 + 内部上拉：
  // 上拉是为了模块没接/没上电时引脚不悬空（悬空读数会乱跳，容易误判成线接错）。
  // 上拉约 45kΩ，比模块板上那颗 10k 弱得多 —— 模块正常供电时它说了算，不干扰。
  // 代价：模块没接时页面会稳定显示「暗」，这是预期不是 bug。
  // 这是**输入模式**，不驱动任何电平，没有烧板子的风险。
  pinMode(LIGHT_PIN, INPUT_PULLUP);

  // 开热点。softAP 内部默认分配 192.168.4.1，不用额外配。
  WiFi.softAP(WIFI_SSID, WIFI_PASS);

  // 通配 DNS **必须放在 softAP() 之后** —— 它要绑定到热点 IP，
  // 热点还没起来时拿不到 192.168.4.1，start() 会失败。
  //
  // 至于它和 setupServer() 谁先谁后：无所谓。两者都只是「注册」，
  // 真正开始收发要等 loop() 跑起来，那时两边都已经就绪了。
  bool dnsOk = dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  setupServer();

  Serial.println();
  Serial.println("零碳新风智能家居系统 v4");
  Serial.println("热点已启动");
  Serial.print("  热点名：");
  Serial.println(WIFI_SSID);
  Serial.print("  密码：");
  Serial.println(WIFI_PASS);
  Serial.print("  手机连上后访问：http://");
  Serial.println(WiFi.softAPIP());
  Serial.print("  强制门户 DNS：");
  Serial.println(dnsOk ? "已启动（连上可能自动弹页）" : "启动失败 —— 不会自动弹窗，只能手动开地址");
  Serial.print("  光照 DO：GPIO");
  Serial.print(LIGHT_PIN);
  Serial.print("  当前=");
  Serial.println(digitalRead(LIGHT_PIN) == DARK_LEVEL ? "暗" : "亮");
  Serial.println();
}

// ================== 主循环 ==================
void loop() {
  dnsServer.processNextRequest();  // 处理 DNS 查询，同样不能停
  server.handleClient();           // 必须频繁调用，中间不能塞 delay()
}
