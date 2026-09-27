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
 *   ✅ 温湿度           —— 真的（DHT11 + YL-47 模块，见 dht11.h）
 *   ✅ 4 路继电器      —— 真的，已上板点动测过（见 relay.h）
 *   ✅ 语音控制        —— 已并入（见 voice.h）：开灯 / 关灯 / 开风扇 / 关闭风扇
 *                        语音执行完会自动切「手动」模式，免得被自动逻辑改回去
 *   ⬜ 灯 / 抽湿机 / 空调的**负载本身还没买** —— 继电器会「咔哒」动作，
 *     但没有东西真的被控制。演示时要说清这是**模拟负载**
 *     （见 文档/材料清单.md 第三节）。
 *
 *   ※ 语音是**事件**不是读数：voiceTakeCmd() 取走即清空，所以一条指令
 *     只执行一次，不会每轮 loop() 重复触发。
 *   ※ 各传感器采样周期不同：光照随网页请求实时读（快）；
 *     PM2.5 每秒一次、温湿度每 2 秒一次（这两个读一次要阻塞几十毫秒，
 *     只能定时采、用缓存值，见下面对应的采样代码）。
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
#include "settings.h"       // 阈值和模式（存 NVS，断电不丢）
#include "light.h"          // 光照：引脚 + lightInit/lightRead
#include "pm25.h"           // PM2.5：GP2Y1014AU + 转接板
#include "dht11.h"          // 温湿度：DHT11 + YL-47（用 DHTesp 库）
#include "relay.h"          // 4 路继电器（光耦隔离，低电平触发）
#include "voice.h"          // 语音模块 CI1302（串口 UART2，10 条命令词）

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

// ================== 4 路设备的通道号 ==================
// 和 relay.h 的 RELAY_PIN 下标一一对应。改设备分配时改 relay.h，这里跟着改。
enum Dev { DEV_FAN = 0, DEV_LIGHT = 1, DEV_DEHUM = 2, DEV_AC = 3 };

// ================== 传感器读数缓存（RAM，掉电无所谓） ==================
int pm25 = 0;                            // 每秒刷新
int lightPct = 0;                        // 每 200ms 刷新
int8_t dhtTemp = -1;                     // 每 2 秒刷新
int8_t dhtHumi = -1;
// DHT11 初值 -1 = 「还没成功读到过」，网页显示「未接」。
// ★ 读失败时**不覆盖** —— DHT11 靠微秒时序通信，开着 WiFi 会偶发失败，
//   失败就清零的话，网页温湿度会时不时跳成 0，看着像坏了。

const unsigned long PM25_SAMPLE_MS  = 1000;   // PM2.5 每秒一次
const unsigned long LIGHT_SAMPLE_MS = 200;    // 光照 200ms 一次（快，可以勤快点）
// 温湿度的周期在 dht11.h 的 DHT_READ_MS（2 秒）

// 用户可调的阈值和模式 —— setup 里由 settingsLoad() 从 NVS 装载，改动后 settingsSave()。
// ⚠️ 为什么必须存 NVS：普通变量在 RAM 里，断电就没了 —— 用户在网页上设的值
//    会「凭空消失」、重启后回默认。这个坑本项目已经踩过一次（模式断电复位）。
Settings set;

// ================== 统一的 JSON 状态响应 ==================
// 页面每秒来问一次；每个会改状态的接口也用它回话，格式统一好处理。
// ★ 所有数值都取**缓存**，不在这儿现读传感器 —— 那会把网页请求卡住。
void sendState() {
  String json = "{";
  json += "\"pm25\":" + String(pm25);
  json += ",\"mode\":\"" + String(set.mode == 0 ? "auto" : "manual") + "\"";

  // 光照：强度百分比 + 由**用户阈值**推出的暗/亮（同一个数据源，不会打架）
  json += ",\"lightPct\":" + String(lightPct);
  json += ",\"light\":\"" + String(lightPct < set.lightDark ? "dark" : "bright") + "\"";

  // 温湿度（DHT11 读失败时缓存值不更新，-1 = 还没读到过）
  json += ",\"temp\":" + String(dhtTemp);
  json += ",\"humi\":" + String(dhtHumi);

  // 4 路设备：**读真实引脚电平**，不靠变量记「刚才设成了什么」——
  // 变量会骗人，读引脚才是事实（原版固件对风扇就是这么做的）
  json += ",\"devFan\":"   + String(relayGet(DEV_FAN)   ? "1" : "0");
  json += ",\"devLight\":" + String(relayGet(DEV_LIGHT) ? "1" : "0");
  json += ",\"devDehum\":" + String(relayGet(DEV_DEHUM) ? "1" : "0");
  json += ",\"devAc\":"    + String(relayGet(DEV_AC)    ? "1" : "0");

  // 用户可调阈值（回填给网页的滑杆）
  json += ",\"setLight\":"   + String(set.lightDark);
  json += ",\"setPm25On\":"  + String(set.pm25On);
  json += ",\"setPm25Off\":" + String(set.pm25Off);
  json += ",\"setHumi\":"    + String(set.humiOn);
  json += ",\"setTemp\":"    + String(set.tempOn);

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

  // ── 4 路设备开关：/dev?ch=1~4&on=1|0 ──────────────────
  // 用一个路由 + 查询参数，而不是注册 8 条路径 —— 页面 cmd() 直接拼 URL 就行，
  // 以后加第 5 路也不用改这里（relay.h 加一个脚、网页加一行）。
  server.on("/dev", []() {
    int ch = server.arg("ch").toInt() - 1;          // 页面传 1~4，内部用 0~3
    bool on = (server.arg("on") == "1");
    if (ch >= 0 && ch < RELAY_COUNT) {
      relaySet(ch, on);
      Serial.print("  [网页] 通道 ");
      Serial.print(ch + 1);
      Serial.print(" → ");
      Serial.println(on ? "吸合" : "释放");
    }
    sendState();
  });

  // ── 保存阈值：/set?light=&pm25on=&pm25off=&humi=&temp= ──
  // 必须用 hasArg() 判断页面有没有传这个参数：缺了就保持原值。
  // 否则 server.arg() 返回空串、toInt() 得 0，会把用户设的好好的值清成 0。
  server.on("/set", []() {
    if (server.hasArg("light"))   set.lightDark = constrain(server.arg("light").toInt(),   5, 95);
    if (server.hasArg("pm25on"))  set.pm25On    = constrain(server.arg("pm25on").toInt(),   1, 500);
    if (server.hasArg("pm25off")) set.pm25Off   = constrain(server.arg("pm25off").toInt(),  0, 400);
    if (server.hasArg("humi"))    set.humiOn    = constrain(server.arg("humi").toInt(),    20, 95);
    if (server.hasArg("temp"))    set.tempOn    = constrain(server.arg("temp").toInt(),    10, 50);
    // 回差必须成立：关阈值要严格小于开阈值，否则自动逻辑会自相打架
    if (set.pm25Off >= set.pm25On) set.pm25Off = set.pm25On - 1;
    if (!settingsValid(set)) set = settingsDefault();   // 最后兜一道
    settingsSave(set);                                  // ★ 存 NVS，断电不丢
    Serial.println("  [网页] 阈值已保存 → 光照<" + String(set.lightDark) + "%  PM25 "
                   + String(set.pm25Off) + "/" + String(set.pm25On)
                   + "  湿>" + String(set.humiOn) + "%  温>" + String(set.tempOn) + "C");
    sendState();
  });

  // ── 模式切换（也存 NVS）─────────────────────────────
  // 这个坑本项目踩过：只存 RAM 的话，断电重启模式就回「自动」，
  // 用户会觉得"我明明切过手动"。存进 NVS 才不会丢。
  server.on("/mode/auto", []() {
    set.mode = 0;
    settingsSave(set);
    sendState();
  });
  server.on("/mode/manual", []() {
    set.mode = 1;
    settingsSave(set);
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

  // ★ 先从 NVS 加载设置 —— 后面所有初始化和自动逻辑都依赖它
  set = settingsLoad();

  lightInit();   // 光照 AO：输入模式，不驱动电平（细节见 light.h）
  pm25Init();    // PM2.5：LED 脚设为输出并熄灭、AO 脚设为输入（细节见 pm25.h）
  dht11Init();   // 温湿度：注册 DHT11 型号，并等 1 秒（手册要求，详见 dht11.h）
  relayInit();   // 4 路继电器：**先写输出锁存器再切 OUTPUT**，
                 // 否则上电瞬间会全吸一下（详见 relay.h）
  voiceInit();   // 语音模块：开 Serial2（GPIO16/17，115200）。
                 // 上电握手由 loop() 里的 voicePoll() 自动应答（详见 voice.h）

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
    int pct = 0;
    lightRead(pct);
    Serial.print(pct);
    Serial.print("%  判定=");
    Serial.print(pct < set.lightDark ? "暗" : "亮");   // 用用户阈值判定
  }
  Serial.println();
  // ★ 引脚不写死 —— 从 relay.h 的 RELAY_PIN 读，改脚时这里自动跟着变
  Serial.print("  继电器：");
  for (int i = 0; i < RELAY_COUNT; i++) {
    if (i) Serial.print("  ");
    Serial.print("IN");
    Serial.print(i + 1);
    Serial.print("=GPIO");
    Serial.print(RELAY_PIN[i]);
  }
  Serial.print("  初态=");
  for (int i = 0; i < RELAY_COUNT; i++) Serial.print(relayGet(i) ? "吸" : "放");
  Serial.println("  （应该全「放」）");
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
  Serial.print("  温湿度：DATA=GPIO");
  Serial.print(DHT_PIN);
  Serial.print("  首次读数=");
  {
    int8_t t = 0, h = 0;
    if (dht11Read(t, h)) {          // setup 里读一次没问题，此时还没开始服务网页
      dhtTemp = t; dhtHumi = h;
      Serial.print(t);
      Serial.print(" C / ");
      Serial.print(h);
      Serial.print(" %RH");
    } else {
      Serial.print("读失败（正常现象 —— DHT11 偶发失败，后面会自动重试）");
    }
  }
  Serial.println();
  Serial.print("  语音：RX=GPIO");
  Serial.print(VOICE_RX_PIN);
  Serial.print("  TX=GPIO");
  Serial.print(VOICE_TX_PIN);
  Serial.println("  115200");
  Serial.println("        能听懂的指令：你好小丹 / 开灯 / 开风扇 / 关灯 / 关闭风扇");
  Serial.println("        上电握手（A5 FA 00 80 0A 00 21 FB）会自动回 ACK");
  Serial.println("        语音指令执行后自动切「手动」模式，免得被自动逻辑改回去");
  Serial.println();
  Serial.println();
}

// ================== 主循环 ==================
void loop() {
  dnsServer.processNextRequest();  // 处理 DNS 查询，同样不能停
  server.handleClient();           // 必须频繁调用，中间不能塞 delay()

  unsigned long now = millis();

  // ── 光照：每 200ms 采一次（快，没有阻塞问题）─────────────
  static unsigned long lastLightMs = 0;
  if (now - lastLightMs >= LIGHT_SAMPLE_MS) {
    lastLightMs = now;
    lightRead(lightPct);        // 模块只给强度；「暗/亮」的阈值在 main 这边
  }

  // ── PM2.5：每 1 秒采一次 ────────────────────────────────
  // pm25Read() 阻塞约 10ms（必须等够 LED 的 10ms 脉冲周期），所以：
  //   · 只能在这里定时采 —— **绝不能放进 sendState()**，那会把网页请求卡住
  //   · 每秒阻塞 10ms ≈ 1%；handleClient() 一秒被调上千次，这点时间无感
  static unsigned long lastPm25Ms = 0;
  if (now - lastPm25Ms >= PM25_SAMPLE_MS) {
    lastPm25Ms = now;
    pm25 = pm25Read();
  }

  // ── DHT11 温湿度：每 2 秒读一次 ──────────────────────────
  // DHT 读一次要几十毫秒（主机先拉低 18ms 等它对答），同样只能定时采。
  // ★ 失败时什么都不做 —— **不覆盖** dhtTemp/dhtHumi，保留上一次成功的读数。
  //   DHT11 靠微秒时序，开着 WiFi 会偶发失败；失败就清零会让网页温湿度
  //   时不时跳成 0。初值 -1 = 还没成功读到过，网页显示「未接」。
  static unsigned long lastDhtMs = 0;
  if (now - lastDhtMs >= DHT_READ_MS) {
    lastDhtMs = now;
    int8_t t = 0, h = 0;
    if (dht11Read(t, h)) {
      dhtTemp = t;
      dhtHumi = h;
    }
  }

  // ── 语音：收指令并执行 ──────────────────────────────────
  // ★ voicePoll() 非阻塞：每轮把串口缓冲里的字节读完就返回（一帧 8 字节
  //   在 115200 下约 0.7ms）。**绝不能在这里 delay() 等待**，否则
  //   handleClient() 被堵住，网页就没响应了。
  voicePoll();

  // voiceTakeCmd() 是「取走」语义：拿到一条就清掉，所以只会执行一次，
  // 不会每轮 loop() 都重复触发同一条指令。
  int8_t vc = voiceTakeCmd();
  if (vc != VC_NONE) {
    switch (vc) {
      case VC_LIGHT_ON:  relaySet(DEV_LIGHT, true);  break;   // 开灯   → IN2
      case VC_LIGHT_OFF: relaySet(DEV_LIGHT, false); break;   // 关灯   → IN2
      case VC_FAN_ON:    relaySet(DEV_FAN,   true);  break;   // 开风扇 → IN1
      case VC_FAN_OFF:   relaySet(DEV_FAN,   false); break;   // 关风扇 → IN1
      default: break;   // 唤醒词 / 欢迎语 / 休息语：voice.h 里已打日志，不动设备
    }

    // ★ 语音算「手动操作」—— 执行完立刻切手动模式。
    //   不切的话会重演原版固件那个坑：语音刚关掉的风扇，下一轮自动判据
    //   又给开回来（CLAUDE.md 已知的坑第 5 条）。
    //   所以这段必须放在**自动控制之前** —— 同一次 loop() 走到下面时
    //   set.mode 已经是 1，自动那段就跳过了。
    if (vc == VC_LIGHT_ON || vc == VC_LIGHT_OFF ||
        vc == VC_FAN_ON   || vc == VC_FAN_OFF) {
      if (set.mode != 1) {
        set.mode = 1;
        settingsSave(set);        // 存 NVS：断电重启也还是手动
        Serial.println("  [语音] 已切到手动模式（免得被自动逻辑改回去）");
      }
    }
  }

  // ── 自动控制：只在自动模式下跑 ───────────────────────────
  // 4 路一一对应，每路都带回差 —— 否则数值在阈值附近抖动时，
  // 继电器会反复咔哒，又吵又伤触点（这是照搬原版固件对风扇的设计）。
  //
  // ★ 手动模式（set.mode == 1）时整段跳过，网页按钮说了算。
  if (set.mode == 0) {
    // ① 进风 + 排风扇 ← PM2.5（开/关双阈值）
    if (pm25 > set.pm25On)        relaySet(DEV_FAN, true);
    else if (pm25 < set.pm25Off)  relaySet(DEV_FAN, false);

    // ② 灯 ← 光照（低于阈值开灯，回差 5%）
    if (lightPct < set.lightDark)          relaySet(DEV_LIGHT, true);
    else if (lightPct > set.lightDark + 5) relaySet(DEV_LIGHT, false);

    // ③ 抽湿机 ← 湿度（高于阈值开，回差 5%）
    //    dhtHumi = -1 表示 DHT11 还没成功读到过，这时不动
    if (dhtHumi >= 0) {
      if (dhtHumi > set.humiOn)          relaySet(DEV_DEHUM, true);
      else if (dhtHumi < set.humiOn - 5) relaySet(DEV_DEHUM, false);
    }

    // ④ 空调 ← 温度（高于阈值开，回差 1°C）
    if (dhtTemp >= 0) {
      if (dhtTemp > set.tempOn)          relaySet(DEV_AC, true);
      else if (dhtTemp < set.tempOn - 1) relaySet(DEV_AC, false);
    }
  }
}
