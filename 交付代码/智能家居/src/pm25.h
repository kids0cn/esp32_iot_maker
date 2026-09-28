// pm25.h —— PM2.5 传感器 GP2Y1014AU + 转接板：ILED 给采样脉冲，AO 读电压
// 注意：转接板 VCC 必须 5V；采样脉冲周期按手册 10ms（280+40+9680µs）。

#pragma once

#include <Arduino.h>

const int PM25_LED_PIN  = 33;
const int PM25_AN_PIN   = 32;
const int LED_ON_LEVEL  = HIGH;
const int LED_OFF_LEVEL = LOW;

const int SAMPLING_US   = 280;
const int PULSE_TAIL_US = 40;
const int SLEEP_US      = 9680;

const float ADC_MAX         = 4095.0;
const float ADC_VREF_MV     = 3300.0;
const float DIVIDER_RESTORE = 11.0;
const float NO_DUST_MV      = 1562.0;
const float COV_RATIO       = 0.20f;

const int PM25_FILTER_N = 10;

inline void pm25Init() {
  pinMode(PM25_LED_PIN, OUTPUT);
  digitalWrite(PM25_LED_PIN, LED_OFF_LEVEL);
  pinMode(PM25_AN_PIN, INPUT);
}

inline int pm25FilterAvg(int sample) {
  static int  buf[PM25_FILTER_N];
  static int  idx = 0;
  static long sum = 0;
  static bool filled = false;

  if (!filled) {
    for (int i = 0; i < PM25_FILTER_N; i++) buf[i] = sample;
    sum = (long)sample * PM25_FILTER_N;
    idx = 0;
    filled = true;
    return sample;
  }
  sum -= buf[idx];
  buf[idx] = sample;
  sum += buf[idx];
  idx = (idx + 1) % PM25_FILTER_N;
  return (int)(sum / PM25_FILTER_N);
}

inline int pm25Read(float* voltageMvOut = nullptr) {
  digitalWrite(PM25_LED_PIN, LED_ON_LEVEL);
  delayMicroseconds(SAMPLING_US);
  int pinMv = (int)analogReadMilliVolts(PM25_AN_PIN);
  delayMicroseconds(PULSE_TAIL_US);
  digitalWrite(PM25_LED_PIN, LED_OFF_LEVEL);
  delayMicroseconds(SLEEP_US);

  int mvFiltered = pm25FilterAvg(pinMv);

  float vo = mvFiltered * DIVIDER_RESTORE;
  if (voltageMvOut) *voltageMvOut = vo;

  float density = 0;
  if (vo > NO_DUST_MV) density = (vo - NO_DUST_MV) * COV_RATIO;
  return (int)density;
}
