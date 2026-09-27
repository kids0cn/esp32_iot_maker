#pragma once
// 零碳新风智能家居系统 —— 手机端网页（HTML + CSS + JS 全在这一份里）
//
// 这是**全仓库唯一的网页源**：工程编译用它，工具/生成网页预览.py 也从它抽。
// 改网页就改这里，改完跑一次 `python3 工具/生成网页预览.py` 同步离线预览。
// （早期的 v2 参考固件已删除，不再有第二份要同步。）
//
// 用原始字符串把 HTML 包起来 —— HTML 里的引号、反斜杠都不用转义，
// 写起来就跟写普通网页一样。ESP32 上 const 数据本来就放在 flash，
// 不需要额外的 PROGMEM 标记。
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
  /* 同一张卡片里放两行时（比如温度+湿度），行之间留点间距 */
  .row + .row{margin-top:12px}
  .dot{display:inline-block; width:9px; height:9px; border-radius:50%;
       margin-right:8px; vertical-align:middle; background:var(--dim)}
  .dot.on{background:var(--ok); box-shadow:0 0 10px var(--ok)}
  .dot.off{background:#475569}
  /* 光照：暗=琥珀（提示该开灯了），亮=绿 */
  .dot.dark{background:#f59e0b; box-shadow:0 0 10px #f59e0b}
  .dot.bright{background:#22c55e; box-shadow:0 0 10px #22c55e}

  /* ── 按钮 ── */
  button{
    font:inherit; font-size:16px; font-weight:600; padding:14px 0; border:none;
    border-radius:13px; color:#fff; cursor:pointer; transition:opacity .2s, transform .1s;
  }
  button:active:not(:disabled){transform:scale(.97)}
  button:disabled{opacity:.28; cursor:not-allowed}
  /* 注：早先风扇卡片那两个大按钮（.on-btn/.off-btn）已删。
     4 路设备行的「开 / 关」小按钮样式在下面 .dev .btns 那一段。 */

  /* ── 设备行（4 路继电器）：名字 + 开 / 关两个按钮 ──
     2026-09-27 改的。原来是「点整行切换」，问题是整行看不出能点、
     也看不出当前是开是关，演示时别人根本不敢点。
     现在每路两个明确按钮，代表**当前状态**的那个填充高亮 —— 一眼就懂。 */
  .dev{padding:4px 0}
  .dev .k{display:flex; align-items:center; color:var(--fg); font-size:15px}
  .dev .btns{display:flex; gap:6px; flex-shrink:0}
  .dev .btns button{
    font-size:13px; padding:8px 15px; border-radius:9px;
    background:#0e1729; border:1px solid var(--line); color:var(--dim);
  }
  .dev .btns button.selOn {background:var(--ok); color:#06121f; border-color:var(--ok)}
  .dev .btns button.selOff{background:#475569;   color:#fff;    border-color:#475569}

  /* ── 设置行（滑杆）── */
  .srow{display:flex; align-items:center; gap:10px; margin-top:14px}
  .srow label{font-size:13px; color:var(--dim); width:62px; flex-shrink:0}
  .srow .val{width:56px; text-align:right; font-size:14px; font-weight:600; color:var(--accent)}
  .srow input[type=range]{
    flex:1; -webkit-appearance:none; appearance:none;
    height:4px; border-radius:999px; background:var(--line); outline:none; margin:0;
  }
  .srow input[type=range]::-webkit-slider-thumb{
    -webkit-appearance:none; appearance:none;
    width:18px; height:18px; border-radius:50%;
    background:var(--accent); border:2px solid var(--bg); cursor:pointer;
  }
  .srow input[type=range]::-moz-range-thumb{
    width:18px; height:18px; border-radius:50%;
    background:var(--accent); border:2px solid var(--bg); cursor:pointer;
  }
  .srow input[type=range]:disabled{opacity:.4}

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
      <span class="k">光照</span>
      <span class="v"><span id="ldot" class="dot off"></span><span id="light">--</span></span>
    </div>
  </div>

  <div class="card">
    <div class="row">
      <span class="k">温度</span>
      <span class="v" id="temp">--</span>
    </div>
    <div class="row">
      <span class="k">湿度</span>
      <span class="v" id="humi">--</span>
    </div>
  </div>

  <div class="card">
    <div class="row"><span class="k">设备</span><span class="v" id="devMode">--</span></div>

    <!-- 每路两个按钮：开 / 关。代表**当前状态**的那个会高亮（JS 里设）。 -->
    <div class="row dev">
      <span class="k"><span class="dot off" id="d0"></span>进风 / 排风扇</span>
      <span class="btns">
        <button type="button" data-ch="1" data-on="1">开</button>
        <button type="button" data-ch="1" data-on="0">关</button>
      </span>
    </div>
    <div class="row dev">
      <span class="k"><span class="dot off" id="d1"></span>灯</span>
      <span class="btns">
        <button type="button" data-ch="2" data-on="1">开</button>
        <button type="button" data-ch="2" data-on="0">关</button>
      </span>
    </div>
    <div class="row dev">
      <span class="k"><span class="dot off" id="d2"></span>抽湿机</span>
      <span class="btns">
        <button type="button" data-ch="3" data-on="1">开</button>
        <button type="button" data-ch="3" data-on="0">关</button>
      </span>
    </div>
    <div class="row dev">
      <span class="k"><span class="dot off" id="d3"></span>空调</span>
      <span class="btns">
        <button type="button" data-ch="4" data-on="1">开</button>
        <button type="button" data-ch="4" data-on="0">关</button>
      </span>
    </div>

    <div id="devHint" class="hint"></div>
  </div>

  <div class="card">
    <div class="row"><span class="k">语音模块</span><span class="v" id="voiceState">--</span></div>
    <div class="row">
      <span class="k">最近指令</span>
      <span class="v" id="voiceLast">--</span>
    </div>
    <div class="hint">先喊「你好小丹」唤醒，再说「开风扇 / 开灯 / 关灯 / 关闭风扇」。
      识别到就直接控制对应设备，并自动切到<b>手动</b>模式。</div>
  </div>

  <div class="card">
    <div class="row"><span class="k">控制模式</span></div>
    <div class="seg">
      <button id="mAuto">自动</button>
      <button id="mManual">手动</button>
    </div>
    <div class="hint" id="modeHint"></div>
  </div>

  <div class="card">
    <div class="row"><span class="k">自动控制阈值</span></div>

    <div class="srow">
      <label for="sLight">光照低于</label>
      <input type="range" id="sLight" min="5" max="95" step="5">
      <span class="val" id="vLight">--</span>
    </div>
    <div class="srow">
      <label for="sPm25On">空气差于</label>
      <input type="range" id="sPm25On" min="20" max="300" step="5">
      <span class="val" id="vPm25On">--</span>
    </div>
    <div class="srow">
      <label for="sPm25Off">空气好于</label>
      <input type="range" id="sPm25Off" min="5" max="150" step="5">
      <span class="val" id="vPm25Off">--</span>
    </div>
    <div class="srow">
      <label for="sHumi">湿度高于</label>
      <input type="range" id="sHumi" min="20" max="95" step="1">
      <span class="val" id="vHumi">--</span>
    </div>
    <div class="srow">
      <label for="sTemp">温度高于</label>
      <input type="range" id="sTemp" min="10" max="50" step="1">
      <span class="val" id="vTemp">--</span>
    </div>

    <div class="hint">拖动松手即生效，并<b>保存到 Flash</b> —— 断电重启也不会丢。自动模式下 4 路由传感器驱动，阈值照样生效。</div>
  </div>
</div>

<script>
(function () {
  var $ = function (id) { return document.getElementById(id); };
  var state = null;

  // 4 路设备 —— key 必须和固件 sendState() 发的字段名一致
  // ch 是页面传给板子的通道号（1~4，板子内部是数组下标 0~3）
  var DEVS = [
    { ch: 1, key: 'devFan'   },
    { ch: 2, key: 'devLight' },
    { ch: 3, key: 'devDehum' },
    { ch: 4, key: 'devAc'    }
  ];

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

  // 「多久之前」换个自然的说法。传进来 -1（从没收到过）返回空串。
  function agoText(sec) {
    if (sec === undefined || sec < 0) return '';
    if (sec < 60)   return sec + ' 秒前';
    if (sec < 3600) return Math.floor(sec / 60) + ' 分钟前';
    return Math.floor(sec / 3600) + ' 小时前';
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

    // 光照（4 线制模块只读 AO：强度与暗/亮都来自这一次读取）
    // 不再显示说明文字 —— 那些是接线/实现细节，不该出现在手机界面上。
    if (s.light === undefined) {
      $('light').textContent = '未接';
      $('ldot').className = 'dot off';
    } else {
      var isDark = (s.light === 'dark');
      $('light').textContent = (s.lightPct === undefined)
        ? (isDark ? '暗' : '亮')
        : (isDark ? '暗' : '亮') + ' · ' + s.lightPct + '%';
      $('ldot').className = 'dot ' + (isDark ? 'dark' : 'bright');
    }

    // 温湿度（DHT11）。固件用 -1 表示「还没成功读到过」——
    // DHT11 开着 WiFi 时会偶发读失败，固件那边失败时**保留上一次的值**，
    // 所以这里的 -1 只会出现在「开机后第一次读成功之前」。
    if (s.temp === undefined || s.temp < 0) {
      $('temp').textContent = '未接';
      $('humi').textContent = '未接';
    } else {
      $('temp').textContent = s.temp + ' °C';
      $('humi').textContent = s.humi + ' %RH';
    }

    // ── 4 路设备状态（固件读的是**真实引脚电平**，不是变量）──
    var auto = (s.mode === 'auto');
    for (var i = 0; i < DEVS.length; i++) {
      var on = s[DEVS[i].key] === '1';
      $('d' + i).className = 'dot ' + (on ? 'on' : 'off');
      $('t' + i).textContent = on ? '运行中' : '已关闭';
      // 自动模式下 4 路全由传感器驱动 —— 置灰并禁点。
      // 否则点了也会被下一轮自动逻辑覆盖回去，徒增困惑。
      var row = document.querySelectorAll('.row.dev')[i];
      if (row) row.classList.toggle('off', auto);
    }
    $('devMode').textContent = auto ? '自动' : '手动';
    $('devHint').textContent = auto
      ? '自动模式：4 路都由传感器驱动，切到「手动」才能点。'
      : '手动模式：点任意一行即可开 / 关该路继电器。';

    // ── 语音模块 ──
    // voiceSynced：模块上电跟我们握手过没有。没握手 = 没接 / 没供电 / 收发接反了。
    // voiceCmd + voiceAgo：最近一条指令和「多久之前」。指令是**事件**，
    // 没有「当前值」可读，只能显示最后一次 —— 所以带上时间才说得清。
    var vOn = (s.voiceSynced === '1');
    $('voiceState').textContent = vOn ? '在线' : '未握手';
    $('voiceState').style.color = vOn ? COLOR.ok.fg : COLOR.bad.fg;
    $('voiceLast').textContent = s.voiceCmd
      ? s.voiceCmd + ' · ' + agoText(s.voiceAgo)
      : '还没收到指令';

    // ── 模式 ──
    $('mAuto').className   = auto ? 'active' : '';
    $('mManual').className = auto ? '' : 'active';
    $('modeHint').textContent = auto
      ? 'PM2.5 差 → 开风扇；光照暗 → 开灯；湿度高 → 开抽湿机；温度高 → 开空调。'
        + '每路都带回差，防止在阈值附近反复启停。'
      : '上面四行说了算，自动逻辑不介入。阈值在下面可以改。';

    // ── 阈值回填到滑杆 ──
    // 只在用户没在拖动时回填（当前聚焦的滑杆跳过），免得和手指抢位置
    setSlider('sLight',   'vLight',   s.setLight,   '%');
    setSlider('sPm25On',  'vPm25On',  s.setPm25On,  ' µg/m³');
    setSlider('sPm25Off', 'vPm25Off', s.setPm25Off, ' µg/m³');
    setSlider('sHumi',    'vHumi',    s.setHumi,    '%');
    setSlider('sTemp',    'vTemp',    s.setTemp,    '°C');
  }

  // 回填一个滑杆的值。用户正在拖的那一个不动。
  function setSlider(id, valId, v, unit) {
    var el = $(id);
    if (!el || document.activeElement === el) return;
    el.value = v;
    $(valId).textContent = v + unit;
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

  // ── 4 路设备：点整行切换 ──
  // 自动模式下行被加了 .off，直接忽略点击（和视觉一致）。
  Array.prototype.forEach.call(document.querySelectorAll('.row.dev'), function (row) {
    row.onclick = function () {
      if (row.classList.contains('off')) return;
      var ch  = row.getAttribute('data-ch');
      var key = row.getAttribute('data-key');
      var on  = state && state[key] === '1';
      cmd('/dev?ch=' + ch + '&on=' + (on ? '0' : '1'));   // 当前是开的就发关，反之亦然
    };
  });

  // ── 模式切换 ──
  $('mAuto').onclick   = function () { cmd('/mode/auto'); };
  $('mManual').onclick = function () { cmd('/mode/manual'); };

  // ── 阈值滑杆 ──
  // 拖动中只更新右边的数字（oninput），**松手才发请求**（onchange）。
  // 用 input 会每移动一像素发一次，拖一下几十个请求，板子和手机都白忙。
  [['sLight',  'light',   'vLight',   '%'],
   ['sPm25On', 'pm25on',  'vPm25On',  ' µg/m³'],
   ['sPm25Off','pm25off', 'vPm25Off', ' µg/m³'],
   ['sHumi',   'humi',    'vHumi',    '%'],
   ['sTemp',   'temp',    'vTemp',    '°C']].forEach(function (p) {
    var el = $(p[0]);
    if (!el) return;
    el.oninput  = function () { $(p[2]).textContent = el.value + p[3]; };
    el.onchange = function () { cmd('/set?' + p[1] + '=' + el.value); };
  });

  refresh();
  setInterval(refresh, 1000);
})();
</script>
</body>
</html>
)rawliteral";
