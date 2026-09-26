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
const int PM25_AN_PIN  = 34;   // 接转接板的 AO。ADC1_CH6，且 GPIO34 是只读输入脚

// ================== LED 极性 ==================
// 厂商两份例程都写「高电平点亮」，所以默认 HIGH。
// 若串口一直显示 raw 恒定不动、点蚊香也没反应 → 改成 LOW 再烧一次。
const int LED_ON_LEVEL  = LOW;
const int LED_OFF_LEVEL = HIGH;

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
int readPM25(int& rawOut, float& voltageMvOut, int& rawInstantOut) {
  digitalWrite(PM25_LED_PIN, LED_ON_LEVEL);   // 点亮传感器内部 LED
  delayMicroseconds(SAMPLING_US);             // 等 280µs，让光路稳定
  int raw = analogRead(PM25_AN_PIN);          // 在脉冲开始后 280µs 处采样
  rawInstantOut = raw;                        // 未经滤波的瞬时值，用来分辨「真 0V」还是「悬空乱跳」
  delayMicroseconds(PULSE_TAIL_US);           // 再亮 40µs，凑满 320µs 脉宽
  digitalWrite(PM25_LED_PIN, LED_OFF_LEVEL);  // 熄灭 LED
  delayMicroseconds(SLEEP_US);                // 补足 10ms 周期

  rawOut = filterAvg(raw);

  // 还原真实电压：ADC 读到的只是 VO 的 1/11，乘回来
  voltageMvOut = rawOut * (ADC_VREF_MV / ADC_MAX) * DIVIDER_RESTORE;

  float density = 0;
  if (voltageMvOut > NO_DUST_MV) {
    density = (voltageMvOut - NO_DUST_MV) * COV_RATIO;
  }
  return (int)density;
}

// ================== 自检判定 ==================
// 每 10 秒判一次，直接给结论，不用自己盯数字
void selfCheck(int minRaw, int maxRaw) {
  Serial.println();
  Serial.println("  ── 自检 ──────────────────────────────");
  Serial.print("    raw 范围 : ");
  Serial.print(minRaw);
  Serial.print(" ~ ");
  Serial.print(maxRaw);
  Serial.println("   （正常应落在 0~450）");

  if (maxRaw >= RAW_DANGER) {
    Serial.println("    ❌ 危险：raw 冲到 " + String(maxRaw) + "，说明 AO 没有分压！");
    Serial.println("       转接板上可能没有 10k/1k。**立刻断电**，别继续接 GPIO34。");
  } else if (maxRaw > RAW_MAX_OK) {
    Serial.println("    ⚠️ raw 超过 450 —— 可能没分压，或 AO 接错了脚。先查线。");
  } else if (maxRaw <= RAW_MIN_OK) {
    Serial.println("    ❌ raw 一直是 0 —— 传感器没有输出。");
    Serial.println("       查：VCC 是不是 5V（不是 3.3V）？GND 共地了吗？AO 接对了没？");
  } else if (minRaw == maxRaw) {
    Serial.println("    ⚠️ raw 恒定不变 —— 传感器可能在跑，但没看到任何光信号变化。");
    Serial.println("       最可能：LED 极性反了 → 把 LED_ON_LEVEL 改成 LOW 再烧一次。");
  } else {
    Serial.println("    ✅ 传感器有响应，读数在动 —— 硬件是通的。");
    Serial.println("       下一步：凑近点根蚊香 / 吹口气，看 raw 会不会明显往上跑。");
  }
  Serial.println("  ──────────────────────────────────────");
  Serial.println();
}

// ================== 开机自检：先确定 LED 到底亮不亮、哪种极性才亮 ==================
// 红外 LED 肉眼看不见，但**手机摄像头能拍到**（有些手机后置有 IR 滤镜，用前置）。
// 这个函数让 ILED 交替常亮各 2 秒 —— 哪一段能看到红外微光，那段就是正确的点亮电平。
// 只在开机跑一次，跑完再进正常采样。
void ledBringUpTest() {
  Serial.println("  ── ILED 接线自检（用手机摄像头对着传感器内部看）──");
  Serial.println("     红外看不见，必须用手机摄像头！有些机子后置有 IR 滤镜，用前置。");
  Serial.println("     现在：ILED 输出【高电平】保持 2 秒…");
  digitalWrite(PM25_LED_PIN, HIGH);
  delay(2000);
  Serial.println("     现在：ILED 输出【低电平】保持 2 秒…");
  digitalWrite(PM25_LED_PIN, LOW);
  delay(2000);
  Serial.println("     ↑ 哪一段看到了红外微光，把 LED_ON_LEVEL 设成那个电平。");
  Serial.println("       两段都看不到 → 不是极性问题，是供电/共地/接线（见下方提示）。");
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
  int raw = 0;
  float voltageMv = 0;
  int rawInstant = 0;
  int pm25 = readPM25(raw, voltageMv, rawInstant);

  // 记录运行区间，用来判断「读数到底会不会动」
  static int minRaw = 4095, maxRaw = 0;
  if (raw < minRaw) minRaw = raw;
  if (raw > maxRaw) maxRaw = raw;

  Serial.print("raw=");
  Serial.print(raw);
  Serial.print("  瞬时=");
  Serial.print(rawInstant);   // 未滤波：恒定 0 = 引脚真是 0V；乱跳 = 引脚悬空
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
    selfCheck(minRaw, maxRaw);
  }

  delay(1000);
}
