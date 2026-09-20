/*
 * 零碳新风系统 —— ESP32 智能家居模拟装置
 * ============================================
 * 来源：提供方提供的现有代码，2026-09-20 入库。**原样保存，未做任何修改。**
 * 库内已知问题与功能覆盖情况见 `文档/项目需求.md` 与 `文档/材料清单.md`。
 *
 * 平台：Arduino (ESP32 core)
 * 板子：ESP-WROOM-32 (CP2102)
 *
 * 硬件接线（据代码注释）：
 *   继电器 IN        -> GPIO 2   (低电平触发)
 *   PM2.5 驱动 LED   -> GPIO 13
 *   PM2.5 模拟输出   -> GPIO 34  (必须加 10k 分压)
 *
 * WiFi 热点：ZeroCarbonFan / 12345678，控制页 http://192.168.4.1
 */

#include <WiFi.h>

// ================== 引脚定义 ==================
const int relayPin = 2;      // 继电器 IN 引脚 (接 GPIO 2)
const int pm25_LED = 13;     // PM2.5传感器 驱动LED引脚 (接 GPIO 13)
const int pm25_AN = 34;      // PM2.5传感器 模拟输出引脚 (接 GPIO 34，必须加10k分压)

// ================== WiFi热点配置 ==================
const char* ssid = "ZeroCarbonFan";      // 手机要连的WiFi名字
const char* password = "12345678";       // 密码（至少8位）

WiFiServer server(80);

// ================== 传感器采样函数 ==================
int getPM25() {
  digitalWrite(pm25_LED, LOW);  // 打开红外LED
  delayMicroseconds(280);       // 等待280微秒
  int val = analogRead(pm25_AN); // 读取电压值
  delayMicroseconds(40);        // 等待40微秒
  digitalWrite(pm25_LED, HIGH); // 关闭红外LED
  delayMicroseconds(968);       // 等待循环周期完成

  // 转换公式（因为加了10k分压，硬件上电压减半，所以软件必须乘2还原）
  float voltage = val * (3.3 / 4095.0) * 2.0;
  float dustDensity = (voltage - 0.6) * 166.67;
  if (dustDensity < 0) dustDensity = 0;
  return (int)dustDensity;
}

void setup() {
  Serial.begin(115200);
  pinMode(relayPin, OUTPUT);
  pinMode(pm25_LED, OUTPUT);

  // 初始状态：继电器高电平（风扇关）
  digitalWrite(relayPin, HIGH);

  // 启动 WiFi 热点
  WiFi.softAP(ssid, password);

  Serial.println();
  Serial.print("WiFi 热点已启动，请在手机浏览器输入: ");
  Serial.println(WiFi.softAPIP());  // 通常打印 192.168.4.1

  server.begin();
}

void loop() {
  // 1. 读取空气数值
  int pm25 = getPM25();

  // 2. 自动控制逻辑
  if (pm25 > 75) {
    digitalWrite(relayPin, LOW);   // 超标，自动开风扇
  }
  else if (pm25 < 50) {
    digitalWrite(relayPin, HIGH);  // 干净，自动关风扇
  }

  // 3. 处理手机网页请求
  WiFiClient client = server.available();
  if (client) {
    String currentLine = "";
    while (client.connected()) {
      if (client.available()) {
        char c = client.read();
        if (c == '\n') {
          if (currentLine.length() == 0) {
            client.println("HTTP/1.1 200 OK");
            client.println("Content-type:text/html");
            client.println("Connection: close");
            client.println();

            client.println("<!DOCTYPE html><html>");
            client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
            client.println("<meta charset='UTF-8'>");
            client.println("<title>零碳新风控制系统</title>");
            client.println("<style>body { font-family: Arial; text-align: center; margin-top: 50px;} button { font-size: 24px; padding: 15px 30px; margin: 10px; border-radius: 10px; border: none; color: white;} </style>");
            client.println("</head><body>");
            client.println("<h1>零碳新风系统</h1>");
            client.println("<p style='font-size: 20px;'>当前 PM2.5 数值: <strong style='color:blue;'>" + String(pm25) + "</strong> ug/m3</p>");

            int relayState = digitalRead(relayPin);
            if (relayState == LOW) {
              client.println("<p style='font-size: 20px; color:green;'>风扇状态: <strong>运行中 (ON)</strong></p>");
            } else {
              client.println("<p style='font-size: 20px; color:red;'>风扇状态: <strong>已停止 (OFF)</strong></p>");
            }

            client.println("<p><a href=\"/ON\"><button style='background-color:green;'>开启风扇</button></a></p>");
            client.println("<p><a href=\"/OFF\"><button style='background-color:red;'>关闭风扇</button></a></p>");
            client.println("</body></html>");
            client.println();
            break;
          } else {
            currentLine = "";
          }
        } else if (c != '\r') {
          currentLine += c;
        }

        if (currentLine.endsWith("GET /ON")) {
          digitalWrite(relayPin, LOW);
        }
        if (currentLine.endsWith("GET /OFF")) {
          digitalWrite(relayPin, HIGH);
        }
      }
    }
    client.stop();
  }

  delay(2000);
}
