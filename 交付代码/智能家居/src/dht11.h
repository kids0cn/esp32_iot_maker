// dht11.h —— 温湿度传感器 DHT11（用 DHTesp 库）
// 注意：VCC 接 3.3V；手册要求上电后等 1 秒才能读。

#pragma once

#include <Arduino.h>
#include "DHTesp.h"

const int DHT_PIN = 13;
const unsigned long DHT_READ_MS = 2000;

static DHTesp dht;

inline void dht11Init() {
  dht.setup(DHT_PIN, DHTesp::DHT11);

  delay(1000);
}

inline bool dht11Read(int8_t& tempOut, int8_t& humiOut) {
  TempAndHumidity th = dht.getTempAndHumidity();
  if (dht.getStatus() != DHTesp::ERROR_NONE) return false;
  tempOut = (int8_t)round(th.temperature);
  humiOut = (int8_t)round(th.humidity);
  return true;
}
