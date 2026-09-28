// main.cpp —— 主程序：开 WiFi 热点 + 网页，读三个传感器，控 4 路继电器，收语音指令
// 注意：loop() 里不能有 delay() —— WebServer 靠频繁调 handleClient() 收发，
//       一旦阻塞网页就没响应；所有定时一律用 millis() 比时间。

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "index_html.h"
#include "settings.h"
#include "light.h"
#include "pm25.h"
#include "dht11.h"
#include "relay.h"
#include "voice.h"

const char* WIFI_SSID = "零碳智能家居";
const char* WIFI_PASS = "";

WebServer server(80);

const byte DNS_PORT = 53;
DNSServer dnsServer;

enum Dev { DEV_FAN = 0, DEV_LIGHT = 1, DEV_DEHUM = 2, DEV_AC = 3 };

int pm25 = 0;
int lightPct = 0;
int8_t dhtTemp = -1;
int8_t dhtHumi = -1;

const unsigned long PM25_SAMPLE_MS  = 1000;
const unsigned long LIGHT_SAMPLE_MS = 200;

bool          voiceHasCmd  = false;
String        voiceCmdText = "";
unsigned long voiceCmdAtMs = 0;

Settings set;

static String json01(bool v) { return String(v ? "\"1\"" : "\"0\""); }

void sendState() {
  String json = "{";
  json += "\"pm25\":" + String(pm25);
  json += ",\"mode\":\"" + String(set.mode == 0 ? "auto" : "manual") + "\"";

  json += ",\"lightPct\":" + String(lightPct);
  json += ",\"light\":\"" + String(lightPct < set.lightDark ? "dark" : "bright") + "\"";

  json += ",\"temp\":" + String(dhtTemp);
  json += ",\"humi\":" + String(dhtHumi);

  json += ",\"devFan\":"   + json01(relayGet(DEV_FAN));
  json += ",\"devLight\":" + json01(relayGet(DEV_LIGHT));
  json += ",\"devDehum\":" + json01(relayGet(DEV_DEHUM));
  json += ",\"devAc\":"    + json01(relayGet(DEV_AC));

  json += ",\"setLight\":"   + String(set.lightDark);
  json += ",\"setPm25On\":"  + String(set.pm25On);
  json += ",\"setPm25Off\":" + String(set.pm25Off);
  json += ",\"setHumi\":"    + String(set.humiOn);
  json += ",\"setTemp\":"    + String(set.tempOn);

  json += ",\"voiceSynced\":" + json01(voiceIsSynced());
  json += ",\"voiceCmd\":\"" + voiceCmdText + "\"";
  json += ",\"voiceAgo\":"    + String(voiceHasCmd ? (int)((millis() - voiceCmdAtMs) / 1000) : -1);

  json += "}";
  server.send(200, "application/json", json);
}

void setupServer() {
  server.on("/", []() {
    server.send(200, "text/html", INDEX_HTML);
  });

  server.on("/api/state", sendState);

  server.on("/dev", []() {
    int ch = server.arg("ch").toInt() - 1;
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

  server.on("/set", []() {
    if (server.hasArg("light"))   set.lightDark = constrain(server.arg("light").toInt(),   5, 95);
    if (server.hasArg("pm25on"))  set.pm25On    = constrain(server.arg("pm25on").toInt(),   1, 500);
    if (server.hasArg("pm25off")) set.pm25Off   = constrain(server.arg("pm25off").toInt(),  0, 400);
    if (server.hasArg("humi"))    set.humiOn    = constrain(server.arg("humi").toInt(),    20, 95);
    if (server.hasArg("temp"))    set.tempOn    = constrain(server.arg("temp").toInt(),    10, 50);

    if (set.pm25Off >= set.pm25On) set.pm25Off = set.pm25On - 1;
    if (!settingsValid(set)) set = settingsDefault();
    settingsSave(set);
    Serial.println("  [网页] 阈值已保存 → 光照<" + String(set.lightDark) + "%  PM25 "
                   + String(set.pm25Off) + "/" + String(set.pm25On)
                   + "  湿>" + String(set.humiOn) + "%  温>" + String(set.tempOn) + "C");
    sendState();
  });

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

  server.on("/favicon.ico", []() {
    server.send(204, "text/plain", "");
  });

  server.onNotFound([]() {
    server.sendHeader("Location",
                      String("http://") + WiFi.softAPIP().toString() + "/",
                      true);
    server.send(302, "text/plain", "");
  });

  server.begin();
}

void setup() {
  Serial.begin(115200);

  set = settingsLoad();

  lightInit();
  pm25Init();
  dht11Init();
  relayInit();

  voiceInit();

  WiFi.softAP(WIFI_SSID, WIFI_PASS);

  bool dnsOk = dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  setupServer();

  Serial.println();
  Serial.println("零碳新风智能家居系统 v4");
  Serial.println("热点已启动");
  Serial.print("  热点名：");
  Serial.println(WIFI_SSID);
  Serial.print("  密码：");

  Serial.println(WIFI_PASS[0] ? WIFI_PASS : "（无 —— 开放网络，连上不用输密码）");
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
    Serial.print(pct < set.lightDark ? "暗" : "亮");
  }
  Serial.println();

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
    int v = pm25Read(&vo);
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
    if (dht11Read(t, h)) {
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
  Serial.println("        能听懂的指令：开灯 / 关灯 / 开风扇 / 关闭风扇 /"
                 " 开抽湿机 / 关抽湿机 / 开空调 / 关空调");
  Serial.println("        （同义说法：打开风扇、关风扇 —— 和上面发的是同一帧）");
  Serial.println("        上电握手（A5 FA 00 80 0A 00 21 FB）会自动回 ACK");
  Serial.println("        语音指令执行后自动切「手动」模式，免得被自动逻辑改回去");
  Serial.println();
  Serial.println();
}

void loop() {
  dnsServer.processNextRequest();
  server.handleClient();

  unsigned long now = millis();

  static unsigned long lastLightMs = 0;
  if (now - lastLightMs >= LIGHT_SAMPLE_MS) {
    lastLightMs = now;
    lightRead(lightPct);
  }

  static unsigned long lastPm25Ms = 0;
  if (now - lastPm25Ms >= PM25_SAMPLE_MS) {
    lastPm25Ms = now;
    pm25 = pm25Read();
  }

  static unsigned long lastDhtMs = 0;
  if (now - lastDhtMs >= DHT_READ_MS) {
    lastDhtMs = now;
    int8_t t = 0, h = 0;
    if (dht11Read(t, h)) {
      dhtTemp = t;
      dhtHumi = h;
    }
  }

  voicePoll();

  int8_t vc = voiceTakeCmd();
  if (vc != VC_NONE) {
    voiceHasCmd  = true;
    voiceCmdText = voiceCmdName();
    voiceCmdAtMs = millis();

    switch (vc) {
      case VC_LIGHT_ON:   relaySet(DEV_LIGHT,  true);  break;
      case VC_LIGHT_OFF:  relaySet(DEV_LIGHT,  false); break;
      case VC_FAN_ON:     relaySet(DEV_FAN,    true);  break;
      case VC_FAN_OFF:    relaySet(DEV_FAN,    false); break;
      case VC_DEHUM_ON:   relaySet(DEV_DEHUM,  true);  break;
      case VC_DEHUM_OFF:  relaySet(DEV_DEHUM,  false); break;
      case VC_AC_ON:      relaySet(DEV_AC,     true);  break;
      case VC_AC_OFF:     relaySet(DEV_AC,     false); break;
      default: break;
    }

    if (voiceCmdIsDevice(vc)) {
      if (set.mode != 1) {
        set.mode = 1;
        settingsSave(set);
        Serial.println("  [语音] 已切到手动模式（免得被自动逻辑改回去）");
      }
    }
  }

  if (set.mode == 0) {
    if (pm25 > set.pm25On)        relaySet(DEV_FAN, true);
    else if (pm25 < set.pm25Off)  relaySet(DEV_FAN, false);

    if (lightPct < set.lightDark)          relaySet(DEV_LIGHT, true);
    else if (lightPct > set.lightDark + 5) relaySet(DEV_LIGHT, false);

    if (dhtHumi >= 0) {
      if (dhtHumi > set.humiOn)          relaySet(DEV_DEHUM, true);
      else if (dhtHumi < set.humiOn - 5) relaySet(DEV_DEHUM, false);
    }

    if (dhtTemp >= 0) {
      if (dhtTemp > set.tempOn)          relaySet(DEV_AC, true);
      else if (dhtTemp < set.tempOn - 1) relaySet(DEV_AC, false);
    }
  }
}
