/*
 * 零碳新风智能家居系统 v2 —— ESP32 智能家居模拟装置
 * ============================================
 * 本文件是 `代码/ZeroCarbonFan/ZeroCarbonFan.ino`（原版）的重写版。
 * 原版原样保留、未改动，两份可以对照着看。
 *
 * 平台：Arduino (ESP32 core)     板子：ESP-WROOM-32 (CP2102)
 *
 * ── 这一版改了什么 ──────────────────────────────
 * 1. 【修 bug】PM2.5 采样脉冲周期 968us → 9680us（详见下方 PM25_SLEEP_US 的说明）
 * 2. 【补功能】新增「自动 / 手动」模式切换，解决手动被自动逻辑覆盖的问题
 * 3. 【换库】裸 WiFiServer → WebServer 库，请求路由清晰，好扩展设备
 * 4. 【换写法】HTML 改用 C++ 的原始字符串（raw string literal），不再逐行 println 拼
 *    —— 注意：正文里不要写出那个定界符的字面形式，否则解析脚本会误判边界
 * 5. 【补功能】新增 /api/state 接口 + 网页 AJAX 轮询，数值每秒自动刷新，不再整页跳转
 * 6. 【去阻塞】去掉 loop() 里的 delay(2000)，改用 millis() 计时，网页响应不再被拖住
 *
 * ── 刻意没改的地方（避免一次动太多、出问题不好定位）──
 *   · 引脚定义、WiFi 名称密码、自动阈值 75/50 —— 全部照旧
 *   · 浓度换算公式的 0.6 偏置和 166.67 系数 —— 照旧
 *     （数据手册 TYP 值是 0.5 和 200，但改它会让同一口空气读数低约 17，
 *       和改时序两件事叠在一起就没法判断是哪个引起的。等时序验证过再单独调。）
 *
 * ── 引脚接线（照旧）──
 *   继电器 IN        -> GPIO 2   (低电平触发)
 *   PM2.5 驱动 LED   -> GPIO 13
 *   PM2.5 模拟输出   -> GPIO 34  (必须加 10k 分压)
 *
 * ── 怎么用 ──────────────────────────────────────
 *   手机连上热点 ZeroCarbonFan（密码 12345678），浏览器访问 http://192.168.4.1
 */

#include <WiFi.h>
#include <WebServer.h>

// ================== 引脚定义 ==================
const int relayPin = 2;    // 继电器 IN 引脚（低电平触发：LOW=吸合=风扇转）
const int pm25_LED = 13;   // PM2.5 传感器 红外 LED 驱动脚
const int pm25_AN  = 34;   // PM2.5 传感器 模拟输出（GPIO34 是只读输入脚，正合适）

// ================== PM2.5 采样时序 ==================
// 依据 Sharp GP2Y1010AU0F/GP2Y1014AU 数据手册「Recommended input condition for LED」：
//   脉冲周期 T = 10 ± 1 ms        ← 关键！三段延时加起来必须凑够 10ms
//   脉冲宽度 PW = 0.32 ± 0.02 ms  ← 即 320us
//   采样时点 = 脉冲开始后 0.28 ms ← 即 280us
// 手册 Note 3 明确要求「按推荐条件驱动 LED 以保证可靠性」，
// 超出规格长期运行会影响 LED 寿命（手册另注：连续工作 5 年光衰 50%）。
const int PM25_SAMPLING_US = 280;   // 开灯后等 280us 再采样
const int PM25_PULSE_TAIL_US = 40;  // 采样完再亮 40us，凑满 320us 脉宽
const int PM25_SLEEP_US = 9680;     // 补足到 10ms 周期（280+40+9680 = 10000us）
// ★ 原版这里写的是 968，少了一位数 → 实际周期只有 1288us，比手册快约 7.8 倍，
//   占空比从 3.2% 变成 25%，既不准确也伤 LED。这一版按手册改回 9680。

// ================== 浓度换算参数 ==================
// 注意：这套换算未经标定，输出是「相对值」，用于判高低够用，不能当仪器读数。
const float ADC_MAX = 4095.0;             // ESP32 analogRead 默认 12 位
const float ADC_VREF = 3.3;               // ESP32 ADC 参考电压
const float DIVIDER_RESTORE = 2.0;        // 硬件上 10k 分压使电压减半，软件乘 2 还原
const float PM25_VOLTAGE_OFFSET = 0.6;    // 无尘时的输出电压（手册 TYP=0.5 / MAX=0.65）
const float PM25_VOLTAGE_TO_UGM3 = 166.67;// 电压→浓度系数（手册 TYP 折算应为 200）

// ================== 自动控制阈值（带回差，照旧）==================
const int PM25_THRESHOLD_ON  = 75;   // 高于此值 → 开风扇
const int PM25_THRESHOLD_OFF = 50;   // 低于此值 → 关风扇
// 50~75 之间：保持当前状态不动。这个「回差」是为了防止数值在阈值附近抖动时
// 风扇反复启停（继电器咔哒咔哒响，又吵又伤触点）。

// ================== 采样周期 ==================
const unsigned long SAMPLE_INTERVAL_MS = 1000;  // 每秒测一次空气

// ================== WiFi 热点配置 ==================
const char* ssid = "ZeroCarbonFan";
const char* password = "12345678";

WebServer server(80);

// ================== 运行状态 ==================
enum Mode { MODE_AUTO, MODE_MANUAL };
Mode mode = MODE_AUTO;          // 开机默认自动模式
int pm25 = 0;                   // 最近一次的 PM2.5 读数
unsigned long lastSampleMs = 0; // 上次采样的时刻

// ================== 继电器控制 ==================
// 低电平触发：写 LOW = 吸合 = 风扇转；写 HIGH = 断开 = 风扇停
void setFan(bool on) {
  digitalWrite(relayPin, on ? LOW : HIGH);
}

// 判断风扇是否在转 —— 直接读引脚电平，而不是靠变量记「刚才设成了什么」。
// 这样即使有别的代码/干扰改了继电器，网页显示的仍是真实状态，不会骗人。
bool fanIsOn() {
  return digitalRead(relayPin) == LOW;
}

// ================== PM2.5 采样 ==================
// 这个函数会阻塞约 10ms（等够脉冲周期），对每秒采样一次来说完全可接受。
// 想做「完全不阻塞」得写成状态机，对这个项目属于过度设计，先不引入。
int readPM25() {
  digitalWrite(pm25_LED, LOW);            // 点亮红外 LED
  delayMicroseconds(PM25_SAMPLING_US);    // 等 280us，让光路稳定
  int raw = analogRead(pm25_AN);          // 采样
  delayMicroseconds(PM25_PULSE_TAIL_US);  // 再亮 40us，凑满 320us 脉宽
  digitalWrite(pm25_LED, HIGH);           // 熄灭 LED
  delayMicroseconds(PM25_SLEEP_US);       // 补足 10ms 周期

  float voltage = raw * (ADC_VREF / ADC_MAX) * DIVIDER_RESTORE;
  float density = (voltage - PM25_VOLTAGE_OFFSET) * PM25_VOLTAGE_TO_UGM3;
  if (density < 0) density = 0;           // 干净空气下算出负数，钳到 0
  return (int)density;
}

// ================== 自动控制逻辑 ==================
void applyAutoControl() {
  if (pm25 > PM25_THRESHOLD_ON) {
    setFan(true);                         // 空气差 → 开风扇
  } else if (pm25 < PM25_THRESHOLD_OFF) {
    setFan(false);                        // 空气好 → 关风扇
  }
  // 中间区间什么都不做 —— 保持原状态，这就是回差
}

// ================== 统一的 JSON 状态响应 ==================
// 网页每秒来问一次，所有会改状态的接口也用它回话，格式统一好处理。
void sendState() {
  String json = "{";
  json += "\"pm25\":" + String(pm25);
  json += ",\"fan\":" + String(fanIsOn() ? "true" : "false");
  json += ",\"mode\":\"" + String(mode == MODE_AUTO ? "auto" : "manual") + "\"";
  json += ",\"on\":" + String(PM25_THRESHOLD_ON);
  json += ",\"off\":" + String(PM25_THRESHOLD_OFF);
  json += "}";
  server.send(200, "application/json", json);
}

// ================== 网页 ==================
// 用原始字符串（raw string literal）写，HTML 怎么写就怎么贴，不用转义引号。
// ESP32 上 PROGMEM 是空的（flash 内存映射），const 数据本来就在 flash 里，不用加。
const char INDEX_HTML[] = R"rawliteral(
<!DOCTYPE html>
<html lang="zh-CN">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>零碳新风智能家居系统</title>
<style>
  :root{
    --bg:#0b1220; --card:#16213a; --line:#243354;
    --fg:#e8eefc; --dim:#8296b8;
    --ok:#22c55e; --warn:#f59e0b; --bad:#ef4444; --accent:#38bdf8;
  }
  *{box-sizing:border-box; -webkit-tap-highlight-color:transparent}
  body{
    margin:0; padding:22px 16px 34px; background:var(--bg); color:var(--fg);
    font-family:system-ui,-apple-system,"PingFang SC","Microsoft YaHei",sans-serif;
  }
  .wrap{max-width:430px; margin:0 auto}
  h1{font-size:19px; text-align:center; margin:0 0 6px; letter-spacing:.08em}
  .conn{text-align:center; font-size:12px; margin-bottom:18px; color:var(--dim)}
  .conn.ok{color:var(--ok)}
  .conn.bad{color:var(--bad)}
  .card{background:var(--card); border-radius:18px; padding:18px; margin-bottom:14px}

  /* ── PM2.5 大数字 ── */
  .pm{text-align:center}
  .pm .label{font-size:13px; color:var(--dim); letter-spacing:.1em}
  .pm .value{font-size:64px; font-weight:700; line-height:1.05; margin:6px 0 2px}
  .pm .unit{font-size:14px; color:var(--dim)}
  .pm .level{display:inline-block; margin-top:12px; padding:5px 16px;
             border-radius:999px; font-size:14px; font-weight:600}
  .bar{height:6px; border-radius:999px; background:var(--line); margin-top:16px; overflow:hidden}
  .bar i{display:block; height:100%; width:0; border-radius:999px; transition:width .5s, background .5s}

  /* ── 风扇状态 ── */
  .row{display:flex; align-items:center; justify-content:space-between}
  .row .k{font-size:15px; color:var(--dim)}
  .row .v{font-size:17px; font-weight:600}
  .dot{display:inline-block; width:9px; height:9px; border-radius:50%;
       margin-right:8px; vertical-align:middle; background:var(--dim)}
  .dot.on{background:var(--ok); box-shadow:0 0 10px var(--ok)}
  .dot.off{background:#475569}

  /* ── 按钮 ── */
  .btns{display:grid; grid-template-columns:1fr 1fr; gap:12px; margin-top:16px}
  button{
    font:inherit; font-size:16px; font-weight:600; padding:14px 0; border:none;
    border-radius:13px; color:#fff; cursor:pointer; transition:opacity .2s, transform .1s;
  }
  button:active:not(:disabled){transform:scale(.97)}
  button:disabled{opacity:.28; cursor:not-allowed}
  .on-btn{background:var(--ok)}
  .off-btn{background:var(--bad)}

  /* ── 模式切换 ── */
  .seg{display:grid; grid-template-columns:1fr 1fr; gap:6px;
       background:#0e1729; border-radius:13px; padding:5px; margin-top:4px}
  .seg button{background:transparent; color:var(--dim); font-size:15px; padding:11px 0; border-radius:9px}
  .seg button.active{background:var(--accent); color:#06121f}
  .hint{font-size:12px; color:var(--dim); margin-top:12px; line-height:1.6}
</style>
</head>
<body>
<div class="wrap">
  <h1>零碳新风智能家居系统</h1>
  <div id="conn" class="conn">连接中…</div>

  <div class="card pm">
    <div class="label">PM2.5</div>
    <div id="pm" class="value">--</div>
    <div class="unit">μg/m³</div>
    <div id="level" class="level">--</div>
    <div class="bar"><i id="bar"></i></div>
  </div>

  <div class="card">
    <div class="row">
      <span class="k">风扇</span>
      <span class="v"><span id="dot" class="dot off"></span><span id="fan">--</span></span>
    </div>
    <div class="btns">
      <button id="btnOn"  class="on-btn">开启风扇</button>
      <button id="btnOff" class="off-btn">关闭风扇</button>
    </div>
    <div id="fanHint" class="hint"></div>
  </div>

  <div class="card">
    <div class="row"><span class="k">控制模式</span></div>
    <div class="seg">
      <button id="mAuto">自动</button>
      <button id="mManual">手动</button>
    </div>
    <div class="hint">
      自动模式：PM2.5 &gt; <b id="thOn">--</b> 自动开风扇，&lt; <b id="thOff">--</b> 自动关。
      中间区间保持不动（回差，防止反复启停）。<br>
      手动模式：由你点按钮决定，自动逻辑不介入。
    </div>
  </div>
</div>

<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  var state = null;

  // 三档颜色写死成具体色值，不用 CSS 变量也不碰 color-mix()——
  // 后者要 Safari 16.2+ / Chrome 111+，演示用的手机若是老机型会掉样式。
  var COLOR = {
    ok:   { fg: '#22c55e', bg: 'rgba(34,197,94,.18)'  },
    warn: { fg: '#f59e0b', bg: 'rgba(245,158,11,.18)' },
    bad:  { fg: '#ef4444', bg: 'rgba(239,68,68,.18)'  }
  };

  // 国标 PM2.5 浓度分级（GB 3095-2012 24 小时均值）
  function grade(v) {
    if (v <= 35)  return ['优', 'ok'];
    if (v <= 75)  return ['良', 'ok'];
    if (v <= 115) return ['轻度污染', 'warn'];
    if (v <= 150) return ['中度污染', 'warn'];
    if (v <= 250) return ['重度污染', 'bad'];
    return ['严重污染', 'bad'];
  }

  function render(s) {
    state = s;

    // PM2.5 数值 + 颜色
    var g = grade(s.pm25);
    var c = COLOR[g[1]];
    $('pm').textContent = s.pm25;
    $('pm').style.color = c.fg;
    $('level').textContent = g[0];
    $('level').style.background = c.bg;
    $('level').style.color = c.fg;
    var pct = Math.min(100, s.pm25 / 250 * 100);
    $('bar').style.width = pct + '%';
    $('bar').style.background = c.fg;

    // 风扇状态
    $('fan').textContent = s.fan ? '运行中' : '已停止';
    $('dot').className = 'dot ' + (s.fan ? 'on' : 'off');

    // 模式
    var auto = (s.mode === 'auto');
    $('mAuto').className   = auto ? 'active' : '';
    $('mManual').className = auto ? '' : 'active';

    // 自动模式下禁用风扇按钮 —— 否则点了也会被下一轮自动逻辑覆盖，
    // 徒增困惑。想手动控制就先切到手动模式，语义清楚，也顺便讲明白了两种模式的区别。
    $('btnOn').disabled  = auto;
    $('btnOff').disabled = auto;
    $('fanHint').textContent = auto
      ? '自动模式下由传感器控制，切到「手动」后可自行操作。'
      : '手动模式：自动逻辑已让位，由你决定。';

    $('thOn').textContent  = s.on;
    $('thOff').textContent = s.off;
  }

  async function refresh() {
    try {
      var r = await fetch('/api/state', { cache: 'no-store' });
      if (!r.ok) throw new Error(r.status);
      render(await r.json());
      $('conn').textContent = '已连接 · 每秒刷新';
      $('conn').className = 'conn ok';
    } catch (e) {
      $('conn').textContent = '连接中断，正在重试…';
      $('conn').className = 'conn bad';
    }
  }

  async function cmd(path) {
    try { await fetch(path, { cache: 'no-store' }); } catch (e) {}
    refresh();   // 立刻拉一次，不等定时器，点下去就有反应
  }

  $('btnOn').onclick  = function () { cmd('/fan/on'); };
  $('btnOff').onclick = function () { cmd('/fan/off'); };
  $('mAuto').onclick   = function () { cmd('/mode/auto'); };
  $('mManual').onclick = function () { cmd('/mode/manual'); };

  refresh();
  setInterval(refresh, 1000);
})();
</script>
</body>
</html>
)rawliteral";

// ================== 网页请求路由 ==================
void setupServer() {
  server.on("/", []() {
    server.send(200, "text/html", INDEX_HTML);
  });

  server.on("/api/state", sendState);   // 网页每秒来问一次

  // 手动控制风扇
  server.on("/fan/on", []() {
    setFan(true);
    sendState();
  });
  server.on("/fan/off", []() {
    setFan(false);
    sendState();
  });

  // 切换控制模式
  server.on("/mode/auto", []() {
    mode = MODE_AUTO;
    sendState();
  });
  server.on("/mode/manual", []() {
    mode = MODE_MANUAL;
    sendState();
  });

  // 浏览器会自动来要图标，直接回「无内容」，免得串口刷一堆 404
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

  pinMode(relayPin, OUTPUT);
  pinMode(pm25_LED, OUTPUT);
  digitalWrite(pm25_LED, HIGH);   // 传感器 LED 先关掉
  setFan(false);                  // 风扇初始关闭

  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.println("零碳新风智能家居系统 v2");
  Serial.print("热点已启动，请连上 ");
  Serial.print(ssid);
  Serial.print(" 后访问 http://");
  Serial.println(WiFi.softAPIP());

  setupServer();
  lastSampleMs = millis();
}

// ================== 主循环 ==================
// 全程不出现 delay()：handleClient() 要频繁被调用，一阻塞网页就没反应了。
void loop() {
  server.handleClient();   // 优先处理网页请求

  unsigned long now = millis();
  if (now - lastSampleMs >= SAMPLE_INTERVAL_MS) {
    lastSampleMs = now;
    pm25 = readPM25();

    if (mode == MODE_AUTO) {
      applyAutoControl();
    }

    // 串口打一行，方便看数据稳不稳、验证采样时序改动后的效果
    Serial.print("PM2.5 = ");
    Serial.print(pm25);
    Serial.print(" ug/m3   模式=");
    Serial.print(mode == MODE_AUTO ? "自动" : "手动");
    Serial.print("   风扇=");
    Serial.println(fanIsOn() ? "开" : "关");
  }
}
