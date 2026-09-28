// settings.h —— 用户可调设置（模式 + 各阈值），存 NVS，断电不丢

#pragma once

#include <Arduino.h>
#include <Preferences.h>

struct Settings {
  int8_t  mode;
  int8_t  lightDark;
  int16_t pm25On;
  int16_t pm25Off;
  int8_t  humiOn;
  int8_t  tempOn;
};

inline Settings settingsDefault() {
  Settings s;
  s.mode      = 0;
  s.lightDark = 50;
  s.pm25On    = 75;
  s.pm25Off   = 35;
  s.humiOn    = 70;
  s.tempOn    = 28;
  return s;
}

inline bool settingsValid(const Settings& s) {
  if (s.mode != 0 && s.mode != 1)               return false;
  if (s.lightDark < 5  || s.lightDark > 95)     return false;
  if (s.pm25On    < 1  || s.pm25On > 500)       return false;
  if (s.pm25Off   < 0  || s.pm25Off >= s.pm25On) return false;
  if (s.humiOn    < 20 || s.humiOn > 95)        return false;
  if (s.tempOn    < 10 || s.tempOn > 50)        return false;
  return true;
}

inline Settings settingsLoad() {
  Settings s = settingsDefault();
  Preferences prefs;
  prefs.begin("setup", false);
  prefs.getBytes("cfg", &s, sizeof(s));
  prefs.end();
  if (!settingsValid(s)) return settingsDefault();
  return s;
}

inline void settingsSave(const Settings& s) {
  Preferences prefs;
  prefs.begin("setup", false);
  prefs.putBytes("cfg", &s, sizeof(s));
  prefs.end();
}
