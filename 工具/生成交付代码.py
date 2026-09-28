#!/usr/bin/env python3
"""从 PlatformIO 工程生成「交付代码」包 —— 只留跑得起来的代码，去掉详细注释。

为什么要这个脚本
----------------
`代码/ZeroCarbonFan_v2/ZeroCarbon/` 那份是**教学版**：注释写得又长又啰嗦，
解释「为什么这么写」、踩过什么坑。那份适合学习，但不适合当交付物 ——
交给别人时对方只想看到能跑的程序。

这个脚本把它转成 `交付代码/智能家居/`：
  · 只留主程序（`[env:esp32dev]`）真正需要的文件，测试程序一个都不带
  · 去掉全部 C++ 注释，只留**每个文件头部一两行**的职责说明 + 硬件雷区提醒
  · `platformio.ini` 只留 `esp32dev` 一个 env

怎么保证「只删注释、不改代码」
------------------------------
1. 用状态机逐字符扫，**字符串字面量和 raw string 里的内容一律不碰** ——
   这一点很关键：`include/index_html.h` 里整张网页都在 `R"rawliteral(...)"` 里，
   HTML/JS 里也有 `//` 和 `<!-- -->`，当成 C++ 注释删掉就把网页毁了。
2. 生成完自己验两道（见 main）：
   · 用桩头文件 g++ 编一遍交付代码 —— 编不过就报错退出
   · 把两边的 `INDEX_HTML` 原始字符串抠出来逐字节比 —— 不一致就报错退出

用法：
    python3 工具/生成交付代码.py
输出：
    交付代码/智能家居/{platformio.ini, src/*, include/*}
"""

import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parent.parent
SRC = ROOT / "代码" / "ZeroCarbonFan_v2" / "ZeroCarbon"
OUT = ROOT / "交付代码" / "智能家居"

# 桩头文件目录（本机 WSL2 没有 ESP32 工具链，只能用桩做语法/类型检查）。
# 没有它也不影响生成，只是跳过编译验证那一步。
STUB = pathlib.Path("/tmp/ardstub2")

# 只带主程序用得到的文件。加模块就往这里加一行 —— 和 main.cpp 的 include 对齐。
HEADERS = ["settings.h", "light.h", "pm25.h", "dht11.h", "relay.h", "voice.h"]

# 每个文件保留的头部：一行职责 + 最多两行硬件雷区。
# ★ 只放「删掉了会出事」的内容（接错烧板子、上电误吸合、网页没响应那种），
#   解释原理的话一律不留 —— 那正是这个脚本要去掉的东西。
HEAD = {
    "main.cpp": (
        "// main.cpp —— 主程序：开 WiFi 热点 + 网页，读三个传感器，控 4 路继电器，收语音指令",
        ["// 注意：loop() 里不能有 delay() —— WebServer 靠频繁调 handleClient() 收发，",
         "//       一旦阻塞网页就没响应；所有定时一律用 millis() 比时间。"],
    ),
    "settings.h": ("// settings.h —— 用户可调设置（模式 + 各阈值），存 NVS，断电不丢", []),
    "light.h": ("// light.h —— 光敏模块：只读 AO，返回光照强度百分比",
                ["// 注意：模块 VCC 接 3.3V，别接 5V（DO 有上拉到 VCC，会灌进 GPIO）。"]),
    "pm25.h": ("// pm25.h —— PM2.5 传感器 GP2Y1014AU + 转接板：ILED 给采样脉冲，AO 读电压",
               ["// 注意：转接板 VCC 必须 5V；采样脉冲周期按手册 10ms（280+40+9680µs）。"]),
    "dht11.h": ("// dht11.h —— 温湿度传感器 DHT11（用 DHTesp 库）",
                ["// 注意：VCC 接 3.3V；手册要求上电后等 1 秒才能读。"]),
    "relay.h": ("// relay.h —— 4 路继电器（光耦隔离，低电平触发）：引脚表 + relaySet / relayGet",
                ["// 注意：低电平触发（给 LOW 才吸合）；VCC↔JD-VCC 跳线帽必须拔掉；",
                 "//       初始化必须**先写输出锁存器、再 pinMode(OUTPUT)**，否则上电会全吸一下。"]),
    "voice.h": ("// voice.h —— 语音模块 CI1302（串口 UART2 · 115200 · 8 字节协议帧）",
                ["// 注意：① 上电必须回握手 ACK，否则模块每 0.4 秒重发、还会来回试波特率产生错位字节；",
                 "//       ② 第 6 个字节是厂商命令码、**不是校验和**，只能整帧逐字节比对。"]),
    "index_html.h": ("// index_html.h —— 手机网页（HTML + CSS + JS 全在这一份里）", []),
}

PLATFORMIO_INI = """; PlatformIO 工程配置 —— 智能家居（零碳新风）
;
; 编译烧录：VS Code + PlatformIO 插件，选 env「esp32dev」，点 Build / Upload。
; 串口监视器波特率 115200（必须和代码里 Serial.begin(115200) 一致，否则是乱码）。

[platformio]
default_envs = esp32dev

[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino
monitor_speed = 115200
build_src_filter = +<main.cpp>

lib_deps =
    beegee-tokyo/DHT sensor library for ESPx@^1.19
"""


def strip_cpp_comments(text: str) -> str:
    """去掉 C++ 注释，但**不碰**字符串字面量、字符字面量和 raw string 的内容。

    状态机逐个字符走。顺序很重要：已经在注释里就忽略引号，
    已经在字符串里就忽略 `//` —— 反过来的话，注释里的引号会把状态带偏，
    之后整份文件的注释识别全错位。
    """
    out = []
    i, n = 0, len(text)
    state = 'code'          # code | line | block | str | chr | raw
    raw_end = ''

    while i < n:
        c = text[i]
        two = text[i:i + 2]

        if state == 'line':
            if c == '\n':
                state = 'code'
                out.append(c)
            i += 1
            continue

        if state == 'block':
            if two == '*/':
                state = 'code'
                i += 2
                out.append(' ')        # 用一个空格替掉，免得前后两个 token 粘一起
            else:
                if c == '\n':
                    out.append(c)      # 保留换行，行号不至于全乱
                i += 1
            continue

        if state in ('str', 'chr'):
            out.append(c)
            if c == '\\' and i + 1 < n:     # 转义：连下一个字符一起吃掉
                out.append(text[i + 1])
                i += 2
                continue
            if (state == 'str' and c == '"') or (state == 'chr' and c == "'"):
                state = 'code'
            i += 1
            continue

        if state == 'raw':
            if text.startswith(raw_end, i):
                out.append(raw_end)
                i += len(raw_end)
                state = 'code'
                continue
            out.append(c)
            i += 1
            continue

        # ── state == 'code' ──
        if two == '//':
            state = 'line'
            i += 2
            continue
        if two == '/*':
            state = 'block'
            i += 2
            continue
        if c == '"':
            state = 'str'
            out.append(c)
            i += 1
            continue
        if c == "'":
            state = 'chr'
            out.append(c)
            i += 1
            continue
        m = re.match(r'R"([^(]{0,16})\(', text[i:])     # R"rawliteral( … )rawliteral"
        if m:
            raw_end = ')' + m.group(1) + '"'
            out.append(m.group(0))
            i += len(m.group(0))
            state = 'raw'
            continue
        out.append(c)
        i += 1

    return ''.join(out)


def tidy(s: str) -> str:
    """删完注释会留下一堆空行，收一收（纯排版，不改语义）。"""
    s = re.sub(r'[ \t]+\n', '\n', s)
    prev = None
    while prev != s:
        prev = s
        s = re.sub(r'\{\n(?:[ \t]*\n)+', '{\n', s)              # 块首不留空行
        s = re.sub(r'\n(?:[ \t]*\n)+([ \t]*\})', r'\n\1', s)    # 块尾不留空行
    s = re.sub(r'\n{3,}', '\n\n', s)
    return s.strip() + '\n'


def grab_index_html(path: pathlib.Path) -> str:
    """抠出 INDEX_HTML 的原始字符串内容，用来比对交付版有没有动过网页。"""
    t = path.read_text(encoding="utf-8")
    m = re.search(r'const char INDEX_HTML\[\]\s*=\s*R"rawliteral\((.*?)\)rawliteral";', t, re.S)
    if not m:
        raise SystemExit(f"❌ 在 {path} 里找不到 INDEX_HTML 的原始字符串块")
    return m.group(1)


def build_files() -> list:
    """生成所有交付文件，返回 [(文件名, 原行数, 交付行数)]"""
    stats = []
    files = ["main.cpp"] + HEADERS + ["index_html.h"]

    for name in files:
        is_html = (name == "index_html.h")
        src = SRC / ("include" if is_html else "src") / name
        if not src.exists():
            raise SystemExit(f"❌ 源文件不存在：{src}")

        raw = src.read_text(encoding="utf-8")
        body = tidy(strip_cpp_comments(raw))

        head, notes = HEAD[name]
        dst = OUT / ("include" if is_html else "src") / name
        dst.parent.mkdir(parents=True, exist_ok=True)
        dst.write_text("\n".join([head] + notes) + "\n\n" + body, encoding="utf-8")

        stats.append((name, len(raw.splitlines()),
                      len(dst.read_text(encoding="utf-8").splitlines())))

    (OUT / "platformio.ini").write_text(PLATFORMIO_INI, encoding="utf-8")
    return stats


def verify() -> None:
    """生成完自己验两道。任一不过就退出，别把坏包交出去。"""
    import subprocess

    # ① 网页必须一字未动 —— raw string 是脚本最容易误伤的地方
    a = grab_index_html(SRC / "include" / "index_html.h")
    b = grab_index_html(OUT / "include" / "index_html.h")
    if a != b:
        print(f"❌ 网页内容被改动了（原 {len(a)} 字节 / 交付 {len(b)} 字节）", file=sys.stderr)
        sys.exit(1)
    print(f"  ✅ 网页一字未动（{len(a)} 字节）")

    # ② 交付代码要能过编译器的眼（本机没 ESP32 工具链，用桩头文件代替）
    if not (STUB / "Arduino.h").exists():
        print(f"  ⚠️ 没找到桩头文件 {STUB}，跳过编译验证")
        return
    r = subprocess.run(
        ["g++", "-fsyntax-only", "-std=gnu++17", f"-I{STUB}",
         f"-I{OUT / 'src'}", f"-I{OUT / 'include'}", "-Wall", "-Wextra",
         str(OUT / "src" / "main.cpp")],
        capture_output=True, text=True)
    if r.returncode != 0:
        print("❌ 交付代码编译不过：\n" + r.stderr, file=sys.stderr)
        sys.exit(1)
    print("  ✅ 编译通过（-Wall -Wextra 无警告）")


def main() -> int:
    if not SRC.exists():
        print(f"❌ 找不到工程目录：{SRC}", file=sys.stderr)
        return 1

    stats = build_files()

    print(f"✅ 已生成 {OUT.relative_to(ROOT)}/")
    print(f"   {'文件':<16}{'原来':>6}{'交付':>7}")
    for name, a, b in stats:
        print(f"   {name:<16}{a:>6}{b:>7}")
    print(f"   {'platformio.ini':<16}{'':>6}{'  (只留 esp32dev)':>7}")
    print()
    verify()

    total_a = sum(a for _, a, _ in stats)
    total_b = sum(b for _, b, _ in stats)
    print(f"\n源码 {total_a} 行 → 交付 {total_b} 行"
          f"（去掉 {100 - total_b * 100 // total_a}% 的注释和空行）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
