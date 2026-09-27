#!/usr/bin/env python3
"""从固件源码里抽出内嵌网页，生成可离线点着玩的预览文件。

为什么要这个脚本
----------------
整页 HTML 放在一个 C++ 原始字符串里。这样一来网页没法单独打开预览 ——
想看看改了样式长什么样，得先烧进板子、连热点、开手机。这个脚本把那段
HTML 抠出来，再注入一段假的 fetch（模拟 ESP32 的接口，内含 1 秒轮询的
自动逻辑），生成一个能直接双击用浏览器打开的预览页。

HTML 源头在哪
-------------
工程里的 代码/ZeroCarbonFan_v2/ZeroCarbon/include/index_html.h
—— 这是**实际编译进固件的那份**，改网页就改它。
（早期的 v2 参考固件已删除，脚本不再从那里取。）

注意：生成出来的预览页里有「预览专用」的桩代码和滑杆，**真实固件里没有这些**。

用法：
    python3 工具/生成网页预览.py
输出：
    文档/web页面原型/v2页面.html
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
FIRMWARE = ROOT / "代码" / "ZeroCarbonFan_v2" / "ZeroCarbon" / "include" / "index_html.h"
OUTPUT = ROOT / "文档" / "web页面原型" / "v2页面.html"

# 预览专用：在页面脚本【之前】注入假 fetch，拦截网络请求
MOCK = """
<script>
/* ⚠️ 预览专用桩代码 —— 真实固件里没有这一段。
   它把 fetch 拦下来，用内存里的假状态模拟 ESP32 的响应，
   好让这个页面在离线（没接硬件）时也能点着玩。 */
(function () {
  // ⚠️ 这里的字段名必须和固件 sendState() 发出来的一字不差，
  //    否则页面读不到 → 界面上会出现一堆 undefined。
  //    （2026-09-27 修过一回：原来还是老字段 fan/on/off，
  //      固件早改成了 devFan/setPm25On/…，预览页阈值全是 undefined。）
  var mock = {
    pm25: 42, mode: 'auto',
    light: 'dark', lightPct: 30,
    temp: 26, humi: 58,
    devFan: '1', devLight: '0', devDehum: '0', devAc: '0',
    setLight: 40, setPm25On: 75, setPm25Off: 50, setHumi: 65, setTemp: 28,
    voiceSynced: '1', voiceCmd: '', voiceAgo: -1
  };
  var mockTick = 0;   // 用来让光照每 6 次轮询翻转一次，离线也能看出卡片会变
  var VOICE_WORDS = ['开风扇', '开灯', '关灯', '关闭风扇'];   // 假装识别到的指令
  var voiceTick = 0, voiceIdx = 0;
  var DEV_KEY = ['devFan', 'devLight', 'devDehum', 'devAc'];   // /dev?ch=1~4 对应哪一路
  var SET_KEY = { light: 'setLight', pm25on: 'setPm25On', pm25off: 'setPm25Off',
                  humi: 'setHumi', temp: 'setTemp' };
  window.__mock = mock;
  // 解析 ?a=1&b=2 形式的查询串
  function queryOf(path, mark) {
    var q = {};
    var i = path.indexOf(mark);
    if (i < 0) return q;
    path.slice(i + mark.length).split('&').forEach(function (kv) {
      var a = kv.split('=');
      if (a[0]) q[a[0]] = a[1];
    });
    return q;
  }
  window.fetch = function (path) {
    // 设备开关：/dev?ch=1~4&on=0|1
    var d = queryOf(path, '/dev?');
    var ch = +d.ch;
    if (ch >= 1 && ch <= 4 && d.on !== undefined) mock[DEV_KEY[ch - 1]] = d.on;
    // 阈值：/set?light=40&pm25on=75…（缺的参数固件里也不会动）
    var t = queryOf(path, '/set?');
    Object.keys(t).forEach(function (k) {
      if (SET_KEY[k] !== undefined) mock[SET_KEY[k]] = +t[k];
    });
    if (path.indexOf('/mode/auto')   >= 0) mock.mode = 'auto';
    if (path.indexOf('/mode/manual') >= 0) mock.mode = 'manual';

    if (path.indexOf('/api/state') >= 0) {
      // 自动模式：让 PM2.5 随时间飘，并按**用户设的阈值**带动风扇 ——
      // 复现固件里的双阈值回差（拖滑杆能立刻看出风扇跟着变）
      if (mock.mode === 'auto') {
        mock.pm25 = Math.max(5, Math.min(210, mock.pm25 + (Math.random() * 26 - 13)));
        if (mock.pm25 > mock.setPm25On)  mock.devFan = '1';
        if (mock.pm25 < mock.setPm25Off) mock.devFan = '0';
      }
      // 光照：每 6 次轮询（约 6 秒）在 暗/亮 之间翻转一次，强度百分比跟着变
      mockTick++;
      if (mockTick % 6 === 0) {
        mock.light = (mock.light === 'dark') ? 'bright' : 'dark';
        mock.lightPct = mock.light === 'dark' ? 30 : 85;
      }
      if (mock.mode === 'auto') {
        if (mock.lightPct < mock.setLight)  mock.devLight = '1';
        else                                mock.devLight = '0';
        mock.devDehum = (mock.humi > mock.setHumi) ? '1' : '0';
        mock.devAc    = (mock.temp > mock.setTemp) ? '1' : '0';
      }
      // 语音：每 9 次轮询（约 9 秒）假装识别到一条指令，
      // 中间让「多少秒前」往上走 —— 好看清时间那一段会变
      if (++voiceTick % 9 === 0) {
        mock.voiceCmd = VOICE_WORDS[voiceIdx++ % VOICE_WORDS.length];
        mock.voiceAgo = 0;
      } else if (mock.voiceAgo >= 0) {
        mock.voiceAgo++;
      }
    }
    return Promise.resolve({ ok: true, json: function () { return Promise.resolve(mock); } });
  };
})();
</script>
"""

# 预览专用：底部滑杆，方便手动拉数值看各档配色
PANEL = """
<div style="position:fixed;left:0;right:0;bottom:0;background:#000c;color:#fff;
            font:13px system-ui;padding:10px 14px;display:flex;gap:12px;align-items:center">
  <b style="color:#f59e0b;white-space:nowrap">预览专用</b>
  <span style="white-space:nowrap">PM2.5</span>
  <input id="pv" type="range" min="0" max="250" value="42" style="flex:1">
  <span id="pvv" style="width:2.4em;text-align:right">42</span>
</div>
<script>
  var pv = document.getElementById('pv');
  pv.oninput = function () { document.getElementById('pvv').textContent = pv.value; };
  var hadFetch = window.fetch;
  window.fetch = function (p) {
    return hadFetch(p).then(function (r) {
      return { ok: true, json: function () {
        return r.json().then(function (s) {
          if (p.indexOf('/api/state') >= 0) s.pm25 = +pv.value;
          return s;
        });
      }};
    });
  };
</script>
"""


def main() -> int:
    if not FIRMWARE.exists():
        print(f"❌ 找不到网页源文件：{FIRMWARE}", file=sys.stderr)
        return 1

    src = FIRMWARE.read_text(encoding="utf-8")

    # 锚定到真正的赋值那一行，而不是直接找原始字符串定界符 ——
    # 松散匹配会踩坑：文件头注释里如果提到这个定界符本身，
    # 非贪婪正则就会从注释里开始匹配，把整份源码当成 HTML 抽出来。
    m = re.search(
        r'const char INDEX_HTML\[\]\s*=\s*R"rawliteral\((.*?)\)rawliteral";',
        src, re.S,
    )
    if not m:
        print("❌ 源文件里找不到 INDEX_HTML 的原始字符串块", file=sys.stderr)
        return 1

    html = m.group(1)

    # 兜底自检：抽出来的东西里不该有 C++ 预处理指令
    if "#include" in html or "void setup()" in html:
        print("❌ 抽出的内容像 C++ 源码而不是网页，定界符匹配跑偏了", file=sys.stderr)
        return 1
    if ')' + 'rawliteral"' in html:
        print("❌ HTML 里含有原始字符串的结束定界符，会截断编译", file=sys.stderr)
        return 1

    anchor = "<script>\n(function () {"
    if anchor not in html:
        print(f"❌ 页面脚本锚点变了，找不到 {anchor!r}", file=sys.stderr)
        return 1

    html = html.replace(anchor, MOCK + anchor, 1)
    html = html.replace("</body>", PANEL + "</body>", 1)

    OUTPUT.parent.mkdir(parents=True, exist_ok=True)
    OUTPUT.write_text(html, encoding="utf-8")
    print(f"✅ 已生成 {OUTPUT.relative_to(ROOT)}（{len(html)} 字节，web 页面 {len(m.group(1))} 字节）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
