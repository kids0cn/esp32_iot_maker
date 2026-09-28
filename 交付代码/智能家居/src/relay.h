// relay.h —— 4 路继电器（光耦隔离，低电平触发）：引脚表 + relaySet / relayGet
// 注意：低电平触发（给 LOW 才吸合）；VCC↔JD-VCC 跳线帽必须拔掉；
//       初始化必须**先写输出锁存器、再 pinMode(OUTPUT)**，否则上电会全吸一下。

#pragma once

#include <Arduino.h>

const int RELAY_PIN[4] = { 25, 26, 27, 14 };
const int RELAY_COUNT  = 4;

const int RELAY_ON_LEVEL  = LOW;
const int RELAY_OFF_LEVEL = HIGH;

const char* const RELAY_NAME[4] = { "进风/排风扇", "灯", "抽湿机", "空调" };

inline void relayInit() {
  for (int i = 0; i < RELAY_COUNT; i++) {
    digitalWrite(RELAY_PIN[i], RELAY_OFF_LEVEL);
    pinMode(RELAY_PIN[i], OUTPUT);
  }
}

inline void relaySet(int ch, bool on) {
  if (ch < 0 || ch >= RELAY_COUNT) return;
  digitalWrite(RELAY_PIN[ch], on ? RELAY_ON_LEVEL : RELAY_OFF_LEVEL);
}

inline bool relayGet(int ch) {
  if (ch < 0 || ch >= RELAY_COUNT) return false;
  return digitalRead(RELAY_PIN[ch]) == RELAY_ON_LEVEL;
}
