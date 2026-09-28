// light.h —— 光敏模块：只读 AO，返回光照强度百分比
// 注意：模块 VCC 接 3.3V，别接 5V（DO 有上拉到 VCC，会灌进 GPIO）。

#pragma once

#include <Arduino.h>

const int LIGHT_AO_PIN = 35;

inline void lightInit() {
  pinMode(LIGHT_AO_PIN, INPUT);
}

inline void lightRead(int& pct) {
  int raw = analogRead(LIGHT_AO_PIN);
  pct = 100 - (raw * 100L) / 4095;
  if (pct < 0)   pct = 0;
  if (pct > 100) pct = 100;
}
