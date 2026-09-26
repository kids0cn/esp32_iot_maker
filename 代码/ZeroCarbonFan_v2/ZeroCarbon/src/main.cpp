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
 *   ✅ PM2.5 读数       —— **真的**（GP2Y1014AU + 转接板，见 pm25.h）
 *   ✅ 光照             —— 真的（光敏模块，只读 AO，见 light.h）
 *   ⬜ 风扇开关         —— **只是 RAM 里的一个变量**。没接继电器，
 *                          所以「自动模式下 PM2.5 超标就开风扇」只是把变量翻个面，
 *                          硬件上什么都不会发生。
 *
 *   ※ 两个传感器都只**读**引脚，没有驱动任何输出 —— 在接上继电器之前，
 *     不存在「引脚没核实就输出、烧板子」的风险。
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
#include "light.h"          // 光照模块：引脚/阈值/读取都在 light.h，本文件只做装配
#include "pm25.h"           // PM2.5 模块（GP2Y1014AU + 转接板）

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
int pm25 = 0;                           // 最近一次 PM2.5 读数，由 loop() 每秒刷新一次

const int PM25_THRESHOLD_ON = 75;       // 超过这个值开风扇
const int PM25_THRESHOLD_OFF = 35;      // 低于这个值关风扇（和上面构成回差，防止频繁启停）
// ⚠️ 上面的 50~75 回差，是为了防止数值在阈值附近抖动时继电器反复咔哒
//    （又吵又伤触点）。这是照搬原版固件的设计。
const unsigned long PM25_SAMPLE_MS = 1000;   // PM2.5 采样周期：1 秒一次

// 光照模块已拆到 src/light.h（引脚、阈值、lightInit/lightRead 都在那）。
// 本文件只负责：常量汇总 / setup 装配 / 路由 / 把各模块状态拼成 JSON。
//
// 以后新增传感器/继电器/语音，同样各建一个 .h，在这里 include 一次、
// setup 里 init 一次、sendState 里 read 一次 —— platformio.ini 不用改
// （build_src_filter 只筛 .cpp/.ino，管不到头文件）。

// ================== 统一的 JSON 状态响应 ==================
// 页面每秒来问一次；每个会改状态的接口也用它回话，格式统一好处理。
void sendState() {
  String json = "{";
  json += "\"pm25\":" + String(pm25);
  json += ",\"fan\":" + String(fanOn ? "true" : "false");
  json += ",\"mode\":\"" + String(mode == MODE_AUTO ? "auto" : "manual") + "\"";

  // 光照：交给模块读，暗/亮 与 强度 都来自这一次读取
  int lightPct = 0;
  bool lightDark = false;
  lightRead(lightPct, lightDark);
  json += ",\"light\":\"" + String(lightDark ? "dark" : "bright") + "\"";
  json += ",\"lightPct\":" + String(lightPct);

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

  lightInit();   // 光照 AO：输入模式，不驱动电平（细节见 light.h）
  pm25Init();    // PM2.5：LED 脚设为输出并熄灭、AO 脚设为输入（细节见 pm25.h）

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
  Serial.print("  光照：AO=GPIO");
  Serial.print(LIGHT_AO_PIN);
  Serial.print("（DO 不接）  强度=");
  {
    int pct = 0; bool dark = false;
    lightRead(pct, dark);
    Serial.print(pct);
    Serial.print("%  判定=");
    Serial.print(dark ? "暗" : "亮");
  }
  Serial.println();
  Serial.print("  PM2.5：ILED=GPIO");
  Serial.print(PM25_LED_PIN);
  Serial.print("  AO=GPIO");
  Serial.print(PM25_AN_PIN);
  Serial.print("  首次读数=");
  {
    float vo = 0;
    int v = pm25Read(&vo);    // setup 里读一次没关系 —— 此时还没开始服务网页请求
    Serial.print(v);
    Serial.print(" ug/m3   (VO=");
    Serial.print(vo, 0);
    Serial.print("mV)");
  }
  Serial.println();
  Serial.println();
}

// ================== 主循环 ==================
void loop() {
  dnsServer.processNextRequest();  // 处理 DNS 查询，同样不能停
  server.handleClient();           // 必须频繁调用，中间不能塞 delay()

  // ── PM2.5：每 1 秒采一次 ────────────────────────────────
  // pm25Read() 会阻塞约 10ms（必须等够 LED 的 10ms 脉冲周期），所以：
  //   · 只能在这里定时采 —— **绝不能放进 sendState()**，
  //     那会让每次网页请求都多等 10ms，页面直接变卡
  //   · 每秒阻塞 10ms ≈ 1%；handleClient() 一秒被调上千次，这点时间无感
  //   · 采到的值存进全局 pm25，sendState() 直接用缓存值
  static unsigned long lastPm25Ms = 0;
  unsigned long now = millis();
  if (now - lastPm25Ms >= PM25_SAMPLE_MS) {
    lastPm25Ms = now;
    pm25 = pm25Read();

    // 自动模式下按 PM2.5 控制风扇（带 35~75 回差，见上面的阈值常量）
    // ⚠️ 风扇目前只是 RAM 变量、没接继电器 —— 这里翻面了硬件也不会动。
    if (mode == MODE_AUTO) {
      if (pm25 > PM25_THRESHOLD_ON)       fanOn = true;
      else if (pm25 < PM25_THRESHOLD_OFF) fanOn = false;
    }
  }
}
