#pragma once
/*
 * 用户可调设置 —— 存 NVS（Flash），断电不丢
 * ============================================
 *
 * ── 为什么要存 NVS ────────────────────────────
 * 普通全局变量放在 **RAM** 里，断电或复位就没了 —— 你在网页上设的阈值
 * 会「凭空消失」，重启后又回默认。这个坑本项目已经踩过一次：
 * 当时问「我切换了手动模式，下次进来又会变成自动模式？」，答案是**会**。
 *
 * NVS 是 ESP32 片上专门存键值的 Flash 分区，**掉电不丢**，而且带磨损均衡，
 * 写几万次都没问题 —— 用户每次改设置才写一次，完全不用担心写坏。
 * 用的是 ESP32 Arduino 核心自带的 Preferences 库，**不用装任何外部库**。
 *
 * ── 存了哪些 ──────────────────────────────────
 *   mode       手动 / 自动
 *   lightDark  光照低于它 → 开灯          （%）
 *   pm25On     PM2.5 高于它 → 开风扇
 *   pm25Off    PM2.5 低于它 → 关风扇（回差，防止反复启停）
 *   humiOn     湿度高于它 → 开抽湿机      （%）
 *   tempOn     温度高于它 → 开空调        （°C）
 *
 * ── 合法性兜底 ────────────────────────────────
 * NVS 里可能是垃圾值（断电时写坏、或以后代码改了结构但没改 key）。
 * 所以每次读完都过一遍 settingsValid()，不合法就**整套回退到默认值** ——
 * 宁可用默认值，也不能拿一个乱值去开继电器。
 */

#include <Arduino.h>
#include <Preferences.h>

// ================== 设置结构 ==================
struct Settings {
  int8_t  mode;       // 0 = 自动（传感器说了算）  1 = 手动（人说了算）
  int8_t  lightDark;  // 光照低于它 → 开灯（%）
  int16_t pm25On;     // PM2.5 高于它 → 开风扇
  int16_t pm25Off;    // PM2.5 低于它 → 关风扇
  int8_t  humiOn;     // 湿度高于它 → 开抽湿机（%）
  int8_t  tempOn;     // 温度高于它 → 开空调（°C）
};

// ================== 默认值（首次开机 / 值不合法时用）==================
inline Settings settingsDefault() {
  Settings s;
  s.mode      = 0;     // 自动
  s.lightDark = 50;    // 光照 % 低于它算暗
  s.pm25On    = 75;
  s.pm25Off   = 35;
  s.humiOn    = 70;    // %
  s.tempOn    = 28;    // °C
  return s;
}

// ================== 合法性检查 ==================
// 不是所有情况都该拒绝 —— 关键是**不能让乱值去开继电器**。
inline bool settingsValid(const Settings& s) {
  if (s.mode != 0 && s.mode != 1)               return false;
  if (s.lightDark < 5  || s.lightDark > 95)     return false;
  if (s.pm25On    < 1  || s.pm25On > 500)       return false;
  if (s.pm25Off   < 0  || s.pm25Off >= s.pm25On) return false;   // 关阈值必须 < 开阈值，否则回差反了
  if (s.humiOn    < 20 || s.humiOn > 95)        return false;
  if (s.tempOn    < 10 || s.tempOn > 50)        return false;
  return true;
}

// ================== 读取：setup() 里调一次 ==================
inline Settings settingsLoad() {
  Settings s = settingsDefault();          // 先给默认值
  Preferences prefs;
  prefs.begin("setup", false);             // false = 读写模式
  prefs.getBytes("cfg", &s, sizeof(s));    // 没存过的话这个 key 读不到，s 保持默认值
  prefs.end();
  if (!settingsValid(s)) return settingsDefault();   // 垃圾值兜底
  return s;
}

// ================== 写入：用户改设置时调 ==================
// 用户改一次才写一次，不会高频写 Flash。
inline void settingsSave(const Settings& s) {
  Preferences prefs;
  prefs.begin("setup", false);
  prefs.putBytes("cfg", &s, sizeof(s));
  prefs.end();
}
