# Web 页面方案

> 记录现状、可选做法与推荐路线。
> 建立：2026-09-20 ｜ 依据：`代码/ZeroCarbonFan/ZeroCarbonFan.ino` + ESP32 core 源码核对

## 一、现版页面长什么样

完整静态还原见 [`现版页面.html`](web页面原型/现版页面.html)，可直接用浏览器打开。
截图：[手机](web页面原型/现版页面-手机.png) ｜ [桌面](web页面原型/现版页面-桌面.png)

页面结构极简 —— 从上到下 5 个元素，全部居中：

| 元素 | 样式 | 对应代码 |
|------|------|---------|
| `<h1>零碳新风系统</h1>` | 浏览器默认 h1 | `loop()` 里 `client.println` |
| `当前 PM2.5 数值: **42** ug/m3` | 20px，数字**蓝色**加粗 | 注入 `String(pm25)` |
| `风扇状态: 运行中 (ON)` | 20px，**绿色**；停止时**红色** | 读 `digitalRead(relayPin)` 分支 |
| `[开启风扇]` 按钮 | 24px 字、绿底白字、圆角 10px | `<a href="/ON">` |
| `[关闭风扇]` 按钮 | 同上，红底白字 | `<a href="/OFF">` |

全部样式就一行 CSS：`body` 居中 + `button` 的字号/内边距/圆角/白字。
**没有**配色主题、卡片、图标、页头，**没有**自动刷新，**没有**模式切换开关。

> 前提：手机连上 `ZeroCarbonFan` 热点后访问 `http://192.168.4.1`

## 二、现状的 6 个问题

1. **不自动刷新** —— 数字只在点按钮触发整页重载时才更新，不是实时
2. **没有自动/手动模式切换**（需求 F-07），而且手动会被自动逻辑覆盖
3. **按钮用 `<a href>` 整页跳转** —— 每次点击都重新加载整个页面
4. **HTML 靠逐行 `client.println()` 拼** —— 引号要转义成 `\"`，没有语法高亮，缩进靠人肉
5. **裸 `WiFiServer` 手写 HTTP 头** —— 没有一个正经的请求路由
6. **`loop()` 里 `delay(2000)`** —— 服务器响应被拖慢，且一次只能服务一个客户端

## 三、ESP32 能不能直接用写好的 HTML 文件？

**能。** 不是只能用代码拼。四条路，从"最像现在"到"最像正经 web 项目"：

### 方案 1：维持现状 —— `client.println()` 逐行拼

零依赖，代码最少。但 HTML 一复杂就没法维护。**不建议继续走。**

### 方案 2：Raw String Literal —— 改动最小，立刻摆脱转义地狱

C++11 的原始字符串，HTML 怎么写就怎么贴进去，**不用转义引号**、可以多行、可以缩进：

```cpp
const char PAGE[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    body { font-family: system-ui; text-align: center; }
  </style>
</head>
<body>
  <h1>零碳新风系统</h1>
  ...
</body>
</html>
)rawliteral";
```

配合 `WebServer` 库发送：

```cpp
#include <WebServer.h>
WebServer server(80);
server.on("/", []() { server.send_P(200, "text/html", PAGE); });
```

**仍然是"写在代码里"，但已经可以整块复制粘贴、正常缩进。** 这是最常用的折中做法。

### 方案 3：LittleFS 文件系统 —— 真正的独立 `.html` 文件

把 `index.html` / `style.css` / `app.js` 上传到 ESP32 的 flash 分区，固件只负责"把文件发出去"：

```cpp
#include <LittleFS.h>
LittleFS.begin(true);
server.serveStatic("/", LittleFS, "/index.html");
```

- **好处**：HTML 是真正的独立文件 —— VS Code 里编辑、语法高亮、补全、可拆分 CSS/JS；改页面**不用改固件代码**
- **代价**：多一个上传步骤。Arduino IDE 需要装 `arduino-littlefs-upload` 插件；PlatformIO 用 `pio run --target uploadfs`
- **空间**：默认 4MB 分区方案下文件系统约 1.5MB，放一个网页绰绰有余（一个典型页面几 KB）

**这是"直接引用写好的 html"的标准答案。**

### 方案 4：AJAX 局部刷新 —— 解决"不刷新"的问题

不管 HTML 放哪，要实时更新都得靠这个。后端多开一个返回 JSON 的接口：

```cpp
server.on("/api/state", []() {
  String json = "{\"pm25\":" + String(pm25) + ",\"fan\":" + String(fanState) + "}";
  server.send(200, "application/json", json);
});
```

前端定时拉：

```js
setInterval(async () => {
  const s = await (await fetch('/api/state')).json();
  document.getElementById('pm25').textContent = s.pm25;
}, 2000);
```

页面不再整页跳转，数字自己动 —— 演示效果差别很大。

## 四、推荐路线

**分两步走，先解决挡演示的问题：**

1. **先做方案 2 + 方案 4** —— 用 raw string literal 重写页面（顺手加"自动/手动模式切换"开关），再加一个 `/api/state` 接口做 AJAX 轮询。
   - 改动可控：一个 `.ino` 文件内搞定，不需要装任何插件
   - 同时解决第 1、2、3、4、6 号问题
   - 把裸 `WiFiServer` 换成 `WebServer` 库，路由清晰，后面加设备好加

2. **页面长到需要拆 CSS/JS 时再上方案 3** —— 那时再引入 LittleFS，把 HTML 挪成独立文件。

**判断依据**：现在页面只有 5 个元素，上 LittleFS 的插件安装成本还不划算；等要加灯光、除湿机、电量显示，页面变复杂了再迁。

## 五、实现时要核对的地方

- 本文引用的 API（`serveStatic` / `send_P` / `on` / `send`）已对着 ESP32 core 的 `WebServer.h` 核过签名，但**具体行为要按本机装的 core 版本实测** —— 本机目前还没装 Arduino 环境
- 换成 `WebServer` 库后，`loop()` 里必须调 `server.handleClient()`，且**不能有长 `delay()`**，否则服务器不响应
- 现有代码的 `delay(2000)` 要拆掉，改成用 `millis()` 做非阻塞计时
