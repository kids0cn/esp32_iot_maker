#!/usr/bin/env python3
"""从固件源码里抽出内嵌网页，生成可离线点着玩的预览文件。

为什么要这个脚本
----------------
v2 固件把整页 HTML 放在一个 C++ 原始字符串（R"rawliteral(...)")里。
这样一来网页没法单独打开预览 —— 想看看改了样式长什么样，得先烧进板子、
连热点、开手机。这个脚本把那段 HTML 抠出来，再注入一段假的 fetch
（模拟 ESP32 的接口，内含 1 秒轮询的自动逻辑），生成一个能直接双击
用浏览器打开的预览页。

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
FIRMWARE = ROOT / "代码" / "ZeroCarbonFan_v2" / "ZeroCarbonFan_v2.ino"
OUTPUT = ROOT / "文档" / "web页面原型" / "v2页面.html"

# 预览专用：在页面脚本【之前】注入假 fetch，拦截网络请求
MOCK = """
<script>
/* ⚠️ 预览专用桩代码 —— 真实固件里没有这一段。
   它把 fetch 拦下来，用内存里的假状态模拟 ESP32 的响应，
   好让这个页面在离线（没接硬件）时也能点着玩。 */
(function () {
  var mock = { pm25: 42, fan: true, mode: 'auto', on: 75, off: 50 };
  window.__mock = mock;
  window.fetch = function (path) {
    if (path.indexOf('/fan/on')  >= 0) mock.fan  = true;
    if (path.indexOf('/fan/off') >= 0) mock.fan  = false;
    if (path.indexOf('/mode/auto')   >= 0) mock.mode = 'auto';
    if (path.indexOf('/mode/manual') >= 0) mock.mode = 'manual';
    // 自动模式下让数值随时间飘，复现固件里的回差逻辑
    if (path.indexOf('/api/state') >= 0 && mock.mode === 'auto') {
      mock.pm25 = Math.max(5, Math.min(210, mock.pm25 + (Math.random() * 26 - 13)));
      if (mock.pm25 > mock.on)  mock.fan = true;
      if (mock.pm25 < mock.off) mock.fan = false;
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
        print(f"❌ 找不到固件：{FIRMWARE}", file=sys.stderr)
        return 1

    src = FIRMWARE.read_text(encoding="utf-8")

    # 锚定到真正的赋值那一行，而不是直接找 R"rawliteral( ——
    # 松散匹配会踩坑：文件头注释里如果提到这个定界符本身，
    # 非贪婪正则就会从注释里开始匹配，把整份固件源码当成 HTML 抽出来。
    m = re.search(
        r'const char INDEX_HTML\[\]\s*=\s*R"rawliteral\((.*?)\)rawliteral";',
        src, re.S,
    )
    if not m:
        print('❌ 固件里找不到 INDEX_HTML 的 R"rawliteral(...)" 块', file=sys.stderr)
        return 1

    html = m.group(1)

    # 兜底自检：抽出来的东西里不该有 C++ 预处理指令
    if "#include" in html or "void setup()" in html:
        print("❌ 抽出的内容像固件源码而不是网页，定界符匹配跑偏了", file=sys.stderr)
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
