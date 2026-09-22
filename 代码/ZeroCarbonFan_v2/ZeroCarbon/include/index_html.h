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
