/*
 * PM2.5 粉尘传感器自检 —— Sharp GP2Y1014AU + 转接板（4 线制）
 * ============================================
 * 独立测试程序：只读传感器、往串口打印，不开 WiFi、不碰继电器。
 * 目的就一个：**确认这个传感器到底能不能用**。
 *
 * ── 接线：4 根线，转接板上已含需要的元件，不用外接任何东西 ──
 *
 *   转接板          ESP32
 *   ─────────────────────────────────────────
 *   VCC    ────→   VIN / 5V        ⚠️ 5V，不是 3.3V
 *   GND    ────→   GND             必须共地
 *   AO     ────→   GPIO34          模拟输入（只读脚，正合适）
 *   ILED   ────→   GPIO14          LED 驱动
 *
 *   转接板已经把这些做在板上，**不需要再买电阻**：
 *     · 150Ω + 220µF  供电 RC（手册要求）
 *     · 10k + 1k      模拟分压（VO ÷ 11）
 *
 * ── ⚠️ 上电前确认一件事：板上确实有分压 ──────────
 *   低头看转接板上有没有**两颗电阻（一颗 10k、一颗 1k）**。
 *   依据你给的 接线.txt：「转接板是1K跟10K的电阻分压采集，所以程序计算时要乘11」
 *
 *   · 有分压 → AO 最高约 4V ÷ 11 ≈ 0.36V，接 GPIO34 安全
 *   · 没分压 → AO 最高约 4V，超过 GPIO34 绝对最大 3.6V，**会打坏引脚**
 *
 *   接上后看串口就能反验：raw 落在 **0~450** = 有分压 ✅；
 *   raw 冲到 4000 以上 = **没分压，立刻断电**。
 *
 * ── 参数依据（来自你给的资料，不是猜的） ──────────
 *   文档/传感器/PM2.5传感器 资料/
 *   ├── 例程使用说明.pdf   ← 280µs 采样、1:10 分压要 ×11、含均值滤波
 *   ├── 接线.txt           ← 4 线（VCC/GND/AO/ILED）+「乘11」
 *   └── STM32例程 main.c   ← 完整换算（3300mV / 12位，和 ESP32 一样）
 *
 * ── 怎么读串口 ────────────────────────────────
 *   每秒一行：
 *     raw=123  Vo=396mV  PM2.5=0 ug/m3   [min=118 max=131]
 *   每 10 秒给一次**自检判定**，直接告诉你传感器活没活。
 */

#include <Arduino.h>

// ================== 引脚 ==================
// AO 必须用 **ADC1** 的脚（GPIO32/33/34/35/36/39）——
// GPIO 0/2/4/12/13/14/15/25/26/27 是 ADC2，**WiFi 一开 analogRead 就失效**。
// 本程序不开 WiFi，但这份参数以后要并进开热点的主程序，所以从一开始就守这条。
const int PM25_LED_PIN = 14;   // 接转接板的 ILED（普通数字输出，哪个脚都行）
const int PM25_AN_PIN  = 32;   // 接转接板的 AO。ADC1_CH4

// ================== LED 极性 ==================
// 厂商两份例程都写「高电平点亮」，所以默认 HIGH。
// 若串口一直显示 raw 恒定不动、点蚊香也没反应 → 改成 LOW 再烧一次。
const int LED_ON_LEVEL  = HIGH;
const int LED_OFF_LEVEL = LOW;

// ================== 采样时序（手册要求，别改） ==================
// Sharp 手册「Recommended input condition for LED」：
//   脉冲周期 T  = 10 ± 1 ms       ← 三段延时加起来必须凑够 10ms
//   脉冲宽度 PW = 0.32 ± 0.02 ms  ← 即 320µs
//   采样时点    = 脉冲开始后 0.28 ms
// 例程使用说明.pdf 也写明「严格遵循手册，在 LED 开启后 280µs 处采样」
const int SAMPLING_US   = 280;   // 点亮后等 280µs 再采样
const int PULSE_TAIL_US = 40;    // 采样完再亮 40µs，凑满 320µs 脉宽
const int SLEEP_US      = 9680;  // 补足 10ms（280 + 40 + 9680 = 10000µs）

// ================== 分压还原与换算 ==================
// ⚠️ 未经标定，输出是**相对值**：判高低够用，不能当仪器读数。
const float ADC_MAX         = 4095.0;  // ESP32 analogRead 默认 12 位
const float ADC_VREF_MV     = 3300.0;  // ESP32 ADC 参考电压 3.3V
const float DIVIDER_RESTORE = 11.0;    // ★ 转接板 10k/1k 分压 → 乘 11 还原（不是 2）
const float NO_DUST_MV      = 400.0;   // 无尘时电压 (mV)，每台有差异，可自行校准
const float COV_RATIO       = 0.20f;   // 浓度系数：µg/m³ per mV

// 自检用的范围判据
const int RAW_MIN_OK = 5;      // raw 低于它 = 传感器没输出
const int RAW_MAX_OK = 500;    // raw 高于它 = 大概率板上没分压，危险
const int RAW_DANGER = 3500;   // raw 到了这个量级 = 确定没分压，立刻断电

// ================== 均值滤波 ==================
// 例程使用说明.pdf：「包含均值滤波算法，用于滤除环境杂散光及电源纹波干扰」
const int FILTER_N = 10;
int  filterBuf[FILTER_N];
int  filterIdx = 0;
long filterSum = 0;
bool filterFilled = false;

int filterAvg(int sample) {
  if (!filterFilled) {
    for (int i = 0; i < FILTER_N; i++) filterBuf[i] = sample;
    filterSum = (long)sample * FILTER_N;
    filterIdx = 0;
    filterFilled = true;
    return sample;
  }
  filterSum -= filterBuf[filterIdx];
  filterBuf[filterIdx] = sample;
  filterSum += filterBuf[filterIdx];
  filterIdx = (filterIdx + 1) % FILTER_N;
  return (int)(filterSum / FILTER_N);
}

// ================== 读一次 ==================
// 会阻塞约 10ms（必须等够脉冲周期）。每秒读一次完全无所谓。
//
// ★ 电压换算用 analogReadMilliVolts()，**不用** analogRead()×3.3/4095：
//   analogRead() 是未校准的裸 ADC 值，ESP32 的 ADC 低压段（<0.1V）分辨率极差，
//   经常直接返回 0。而干净空气下 AO 只有约 55mV，正好落在那个死区里。
//   analogReadMilliVolts() 会读芯片出厂烧在 efuse 里的校准系数，把非线性补偿掉，
//   低压段能正常读数 —— 这是 ESP32 专用的读法。
//
//   两个都读出来是为了**诊断**：
//     rawInstant（未校准）= 0 但 pinMv（校准后）= 55  → 实锤是死区问题，传感器是好的
int readPM25(int& rawEquivOut, float& voltageMvOut, int& rawInstantOut, int& pinMvOut) {
  digitalWrite(PM25_LED_PIN, LED_ON_LEVEL);          // 点亮传感器内部 LED
  delayMicroseconds(SAMPLING_US);                    // 等 280µs，让光路稳定
  int rawInstant = analogRead(PM25_AN_PIN);          // 未校准，只留着做对比诊断
  int pinMv      = (int)analogReadMilliVolts(PM25_AN_PIN);  // 校准后的引脚电压(mV)，用它换算
  delayMicroseconds(PULSE_TAIL_US);                  // 再亮 40µs，凑满 320µs 脉宽
  digitalWrite(PM25_LED_PIN, LED_OFF_LEVEL);         // 熄灭 LED
  delayMicroseconds(SLEEP_US);                       // 补足 10ms 周期

  rawInstantOut = rawInstant;
  pinMvOut      = filterAvg(pinMv);                  // 对毫伏做 10 次滑动平均

  // 显示用的「等效 raw」—— 由校准后的毫伏折算，方便和以前的日志对比
  rawEquivOut = (int)(pinMvOut / (ADC_VREF_MV / ADC_MAX));

  // 还原分压：引脚上是 VO ÷ 11，乘 11 得到传感器真实输出 VO(mV)
  voltageMvOut = pinMvOut * DIVIDER_RESTORE;

  float density = 0;
  if (voltageMvOut > NO_DUST_MV) {
    density = (voltageMvOut - NO_DUST_MV) * COV_RATIO;
  }
  return (int)density;
}

// ================== 自检判定 ==================
// 每 10 秒判一次，直接给结论，不用自己盯数字
void selfCheck(int minRaw, int maxRaw, int instMin, int instMax) {
  int swing = instMax - instMin;   // 瞬时值的摆幅

  Serial.println();
  Serial.println("  ── 自检 ──────────────────────────────");
  Serial.print("    raw 范围   : ");
  Serial.print(minRaw);
  Serial.print(" ~ ");
  Serial.print(maxRaw);
  Serial.println("   （平稳时约 50~120，吹烟后上升）");
  Serial.print("    瞬时值摆幅 : ");
  Serial.print(swing);
  Serial.println("   （未滤波的原始 ADC，摆动幅度）");

  // ① 先看引脚是不是悬空 —— 这是最常见的假象，必须第一个排除
  if (swing > 500) {
    Serial.println("    ❌ 瞬时值在大幅乱跳 —— **引脚悬空**！");
    Serial.println("       AO 线没真正接通：查面包板孔位、换根杜邦线、");
    Serial.println("       确认代码里的 PM25_AN_PIN 和实际插的脚是同一个。");
    Serial.println("       ⚠️ 这时候 raw 显示的数值全是噪声，不是真实读数。");
  }
  // ② 再排除没分压（危险情况）
  else if (maxRaw >= RAW_DANGER) {
    Serial.println("    ❌ 危险：raw 冲到 " + String(maxRaw) + "，说明 AO 没有分压！");
    Serial.println("       转接板上可能没有 10k/1k。**立刻断电**，别继续接 GPIO。");
  } else if (maxRaw > RAW_MAX_OK) {
    Serial.println("    ⚠️ raw 超过 450 —— 可能没分压，或 AO 接错了脚。先查线。");
  }
  // ③ 恒 0：有线、但没信号
  else if (maxRaw <= RAW_MIN_OK) {
    Serial.println("    读数是 0，且瞬时值也不动 —— 引脚上有真实电平，但没有信号。两种可能：");
    Serial.println("      (a) 传感器没通电/坏了 → 量转接板 VCC↔GND 是不是 5V；");
    Serial.println("      (b) 传感器正常，但干净空气下 AO 只有约 55mV，");
    Serial.println("          **ESP32 的 ADC 在低压段分辨率极差、常直接读 0**");
    Serial.println("          （转接板的 ÷11 是给 STM32 设计的，ESP32 上会丢低端）。");
    Serial.println("          验证法：点根蚊香，让烟靠近传感器 —— 若 raw 能升起来，就是 (b)。");
  }
  // ④ 恒定但非 0：可能是真实但极低的静态值
  else if (minRaw == maxRaw) {
    Serial.println("    ⚠️ raw 恒定不变 —— 传感器可能在跑，但没看到光信号变化。");
    Serial.println("       把 LED_ON_LEVEL 改成相反的电平再烧一次试试。");
  }
  // ⑤ 正常
  else {
    Serial.println("    ✅ 瞬时值平稳、raw 有变化 —— 传感器在工作。");
    Serial.println("       下一步：点根蚊香 / 吹口气靠近进气口，看 raw 明不明显往上走。");
  }
  Serial.println("  ──────────────────────────────────────");
  Serial.println();
}

// ================== 开机自检：自动判定 LED 极性 ==================
// 红外 LED 肉眼看不见，用手机摄像头拍得到，但更省事的办法是**直接比数字**：
//   在 ILED=高 和 ILED=低 两种状态下各采样取平均，
//   哪种状态下读数明显更高，哪种就是「点亮」。
// 这样不用摄像头、不用改代码重烧，串口直接告诉你该把 LED_ON_LEVEL 设成什么。
//
// ⚠️ LED 不需要脉冲才能亮 —— 它就是个普通红外 LED，给电就亮。
//    脉冲（280/40/9680）只影响「读数准不准」和「LED 寿命」，不影响亮不亮。
//    所以这里常亮 2 秒来测是完全有效的。

// 在当前 ILED 电平下采样 n 次取平均
int sampleAvg(int n) {
  long sum = 0;
  for (int i = 0; i < n; i++) {
    sum += analogRead(PM25_AN_PIN);
    delay(10);
  }
  return (int)(sum / n);
}

void ledBringUpTest() {
  const int N = 30;   // 每档采样次数（30 × 10ms ≈ 0.3 秒）

  Serial.println("  ── ILED 极性自检（不用摄像头，看数字）──");

  digitalWrite(PM25_LED_PIN, HIGH);
  Serial.print("      ILED = 高电平，采样 ");
  Serial.print(N);
  Serial.print(" 次… 平均 raw = ");
  int avgHigh = sampleAvg(N);
  Serial.println(avgHigh);

  digitalWrite(PM25_LED_PIN, LOW);
  Serial.print("      ILED = 低电平，采样 ");
  Serial.print(N);
  Serial.print(" 次… 平均 raw = ");
  int avgLow = sampleAvg(N);
  Serial.println(avgLow);

  int diff = avgHigh - avgLow;
  if (diff < 0) diff = -diff;

  Serial.println();
  if (diff < 5) {
    Serial.println("      ❌ 两种电平下读数几乎一样（差 " + String(diff) + "）");
    Serial.println("         → **LED 没有在工作**，或者传感器没有输出。");
    Serial.println("           这不是极性问题 —— 改 LED_ON_LEVEL 不会有任何作用。");
    Serial.println("           要查的是：转接板 VCC 是不是真 5V、GND 有没有共地、ILED 线通不通。");
    Serial.println("           再不然手动试：把 ILED 线拔下来，分别碰 GND / 3.3V 各 5 秒看 raw 动不动。");
  } else if (avgHigh > avgLow) {
    Serial.println("      ✅ LED 在【高电平】时读数更高（差 " + String(diff) + "）");
    Serial.println("         → 把 LED_ON_LEVEL 设成 HIGH");
  } else {
    Serial.println("      ✅ LED 在【低电平】时读数更高（差 " + String(diff) + "）");
    Serial.println("         → 把 LED_ON_LEVEL 设成 LOW");
  }
  Serial.println("  ──────────────────────────────────────────────");
  Serial.println();
}

// ================== 初始化 ==================
void setup() {
  // 115200 必须和 platformio.ini 的 monitor_speed 一致，否则是乱码
  Serial.begin(115200);

  pinMode(PM25_LED_PIN, OUTPUT);
  digitalWrite(PM25_LED_PIN, LED_OFF_LEVEL);  // 先熄灭，别在启动时白白点亮
  pinMode(PM25_AN_PIN, INPUT);

  Serial.println();
  Serial.println("=== GP2Y1014AU 转接板 · PM2.5 传感器自检 ===");
  Serial.println("  接线：VCC→5V（不是3.3V）  GND→GND  AO→GPIO34  ILED→GPIO14");
  Serial.print("  ILED = GPIO");
  Serial.print(PM25_LED_PIN);
  Serial.print("（");
  Serial.print(LED_ON_LEVEL == HIGH ? "高电平点亮" : "低电平点亮");
  Serial.print("）    AO = GPIO");
  Serial.println(PM25_AN_PIN);
  Serial.println("  分压还原 ×11　无尘基准 400mV　系数 0.20/mV");
  Serial.println();
  Serial.println("  预期：raw 落在 0~450 之间");
  Serial.println();

  ledBringUpTest();   // 开机先做一次 LED 接线自检（下面采样循环接着跑）
}

// ================== 主循环 ==================
// 这里用 delay(1000) 是安全的 —— 本程序没有任何网络要服务，
// delay() 只在 WebServer 那种要频繁 handleClient() 的场景才是坑。
void loop() {
  int rawEquiv = 0;
  float voltageMv = 0;
  int rawInstant = 0;
  int pinMv = 0;
  int pm25 = readPM25(rawEquiv, voltageMv, rawInstant, pinMv);

  // 记录运行区间，用来判断「读数到底会不会动」
  static int minRaw = 99999, maxRaw = -1;
  if (rawEquiv < minRaw) minRaw = rawEquiv;
  if (rawEquiv > maxRaw) maxRaw = rawEquiv;

  // 瞬时值的摆幅 —— 用来分辨「引脚悬空」和「真实的低电平」
  static int instMin = 99999, instMax = -1;
  if (rawInstant < instMin) instMin = rawInstant;
  if (rawInstant > instMax) instMax = rawInstant;

  Serial.print("AO=");
  Serial.print(pinMv);
  Serial.print("mV  等效raw=");
  Serial.print(rawEquiv);
  Serial.print("  瞬时=");
  Serial.print(rawInstant);
  Serial.print("  Vo=");
  Serial.print(voltageMv, 0);
  Serial.print("mV  PM2.5=");
  Serial.print(pm25);
  Serial.print(" ug/m3   [min=");
  Serial.print(minRaw);
  Serial.print(" max=");
  Serial.print(maxRaw);
  Serial.println("]");

  // 每 10 秒给一次明确结论，免得盯着一屏数字不知道好没好
  static int count = 0;
  if (++count % 10 == 0) {
    selfCheck(minRaw, maxRaw, instMin, instMax);
    // 瞬时摆幅按「每 10 秒一个窗口」统计，下一轮重新算 ——
    // 否则开机时抖过一次，后面每次都报「悬空」，就失去意义了。
    instMin = 99999;
    instMax = -1;
  }

  delay(1000);
}
