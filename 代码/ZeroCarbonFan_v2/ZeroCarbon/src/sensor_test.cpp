/*
 * PM2.5 粉尘传感器测试（Sharp GP2Y1014AU 模块）
 * ============================================
 * 独立测试程序：只读传感器、往串口打印，不开 WiFi、不碰继电器。
 * 用来确认「接线对不对、传感器活没活、读数会不会动」。
 *
 * ── 参数依据（全部来自你给的资料，不是猜的） ──────
 *   文档/传感器/PM2.5传感器 资料/
 *   ├── 例程使用说明.pdf      ← 最关键：280µs 采样、1:10 分压要 ×11、含均值滤波
 *   ├── 分压电阻.png          ← 模块电路图：VO→10k→AOUT→1k→GND
 *   ├── 接线.txt              ← // 转接板是1K跟10K的电阻分压采集，所以下面要乘11
 *   └── STM32例程 main.c      ← 完整换算实现（3300mV / 12位，和 ESP32 一样）
 *   抽出的三份已另存在 文档/传感器/GP2Y1014AU_*.{txt,png,c}
 *
 * ── 接线（依据 分压电阻.png + 接线.txt） ────────
 *   传感器 1脚 V-LED  → 模块板内 150Ω 接 5V（板上已含）
 *   传感器 2脚 LED-GND→ GND
 *   传感器 3脚 LED    → GPIO13      ← 由本程序驱动
 *   传感器 4脚 S-GND  → GND
 *   传感器 5脚 VO     → 板内 10k/1k 分压后从 AOUT 引出 → GPIO34
 *   传感器 6脚 VCC    → 5V（手册 Vcc = 5±0.5V，不能接 3.3V）
 *   模块板上已含 C5/C6 = 100µF×2 ≈ 手册要求的 220µF，不用外接
 *
 *   ⚠️ 分压比是 **11 倍**，不是 2 倍！
 *      VO → 10k(R10) → AOUT → 1k(R6) → GND，AOUT = VO/11
 *      所以 ADC 读到的只是真实电压的 1/11，必须乘 11 还原。
 *      照 2 倍算会把电压压到 0.7V 以下，减掉 0.4V 基准后读数几乎恒为 0。
 *
 * ── 怎么读串口输出 ────────────────────────────
 *   raw=  原始 ADC 值。这块板分压后只用到约 0~450，**不是 0~4095**
 *   Vo=   还原后的真实传感器电压（mV），干净空气下应在 400mV 上下
 *   PM2.5= 浓度（相对值，未校准）
 *   [min/max] 运行区间，用来判断读数到底会不会动
 *
 *   · raw 恒为 0        → 传感器没输出，查 5 脚 / 5V 供电
 *   · raw 恒为 4095     → 输出超量程或接错脚
 *   · raw 恒定不动、点蚊香也不变 → **多半是 LED 极性反了**（见下方说明）
 *   · 吹气 / 蚊香烟后 raw 明显上升 → 一切正常 ✅
 *
 * ── ⚠️ LED 极性有冲突，需要你实测确认 ──────────
 *   你给的两份厂商例程（Arduino DustSensor.ino、STM32 main.c）都是
 *   **高电平点亮**；而模块电路图上 pin3 叫 K_LED（K 通常指阴极，
 *   暗示低电平点亮）。两者矛盾，手册里也没有单独说明这一页。
 *
 *   下面做成常量 LED_ON_LEVEL，默认按厂商例程取 HIGH。
 *   烧录后如果读数恒定不动、点蚊香没反应 → 把它改成 LOW 再烧一次。
 *
 * ── 为什么这些数字是「相对值」 ────────────────
 *   例程使用说明.pdf 明确写了：每台传感器无尘电压有个体差异，
 *   要手动修 NO_DUST_MV；K 值（斜率）要配专业检测仪对比才能校准。
 *   所以下面的读数判高低够用，**不能当仪器读数**。
 */

#include <Arduino.h>

// ================== 引脚 ==================
const int PM25_LED_PIN = 13;   // 传感器 3 脚 K_LED
const int PM25_AN_PIN  = 34;   // 传感器 5 脚分压后的 AOUT。GPIO34 是只读输入脚

// ================== LED 极性（见上方说明，实测确认） ==================
// 1 = 高电平点亮（厂商两份例程的做法，当前默认）
// 0 = 低电平点亮（若读数恒定不动就改成这个）
const int LED_ON_LEVEL = HIGH;
const int LED_OFF_LEVEL = LOW;

// ================== 采样时序 ==================
// 依据 Sharp GP2Y1014AU 手册「Recommended input condition for LED」，
// 且 例程使用说明.pdf 写明「严格遵循手册，在 LED 开启后 280µs 处采样」：
//   脉冲周期 T  = 10 ± 1 ms      ← 三段延时加起来必须凑够 10ms
//   脉冲宽度 PW = 0.32 ± 0.02 ms ← 即 320µs
//   采样时点    = 脉冲开始后 0.28 ms
const int SAMPLING_US   = 280;   // 点亮后等 280µs 再采样
const int PULSE_TAIL_US = 40;    // 采样完再亮 40µs，凑满 320µs 脉宽
const int SLEEP_US      = 9680;  // 补足 10ms（280 + 40 + 9680 = 10000µs）

// ================== 分压还原与换算 ==================
// ⚠️ 未经标定，输出是**相对值**：判高低够用，不能当仪器读数。
const float ADC_MAX         = 4095.0;  // ESP32 analogRead 默认 12 位（STM32 例程用 4096，差 0.03% 可忽略）
const float ADC_VREF_MV     = 3300.0;  // ESP32 ADC 参考电压 3.3V，和 STM32 例程一致
const float DIVIDER_RESTORE = 11.0;    // ★ 模块 10k/1k 分压 → 必须乘 11 还原（不是 2）
const float NO_DUST_MV      = 400.0;   // 无尘时的电压 (mV)。例程值 400，每台需自行校准
const float COV_RATIO       = 0.20f;   // 浓度系数：µg/m³ per mV（等价于 200 µg/m³ per V）

// ================== 均值滤波 ==================
// 例程使用说明.pdf：「包含均值滤波算法，用于滤除环境杂散光及电源纹波干扰」。
// 10 次滑动平均 —— 和厂商两份例程一致。
const int FILTER_N = 10;
int filterBuf[FILTER_N];
int filterIdx = 0;
long filterSum = 0;
bool filterFilled = false;

int filterAvg(int sample) {
  if (!filterFilled) {
    // 首次进来先把缓冲区填满，避免从 0 开始爬升导致开头一段读数偏低
    for (int i = 0; i < FILTER_N; i++) filterBuf[i] = sample;
    filterSum = (long)sample * FILTER_N;
    filterIdx = 0;
    filterFilled = true;
    return sample;
  }
  filterSum -= filterBuf[filterIdx];   // 去掉最老的一个
  filterBuf[filterIdx] = sample;       // 放进最新的
  filterSum += filterBuf[filterIdx];
  filterIdx = (filterIdx + 1) % FILTER_N;
  return (int)(filterSum / FILTER_N);
}

// ================== 读一次 ==================
// 会阻塞约 10ms（必须等够脉冲周期）。每秒读一次完全无所谓；
// 但这个函数不能放进要频繁 handleClient() 的循环里。
int readPM25(int& rawOut, float& voltageMvOut) {
  digitalWrite(PM25_LED_PIN, LED_ON_LEVEL);  // 点亮传感器内部 LED
  delayMicroseconds(SAMPLING_US);            // 等 280µs，让光路稳定
  int raw = analogRead(PM25_AN_PIN);         // 在脉冲开始后 280µs 处采样
  delayMicroseconds(PULSE_TAIL_US);          // 再亮 40µs，凑满 320µs 脉宽
  digitalWrite(PM25_LED_PIN, LED_OFF_LEVEL); // 熄灭 LED
  delayMicroseconds(SLEEP_US);               // 补足 10ms 周期

  rawOut = filterAvg(raw);

  // 还原真实电压：ADC 读到的只是 VO 的 1/11，要乘回来
  voltageMvOut = rawOut * (ADC_VREF_MV / ADC_MAX) * DIVIDER_RESTORE;

  // 电压 → 浓度（在 mV 空间算，和 STM32 例程写法一致）
  float density = 0;
  if (voltageMvOut > NO_DUST_MV) {
    density = (voltageMvOut - NO_DUST_MV) * COV_RATIO;
  }
  return (int)density;
}

// ================== 初始化 ==================
void setup() {
  // 115200 必须和 platformio.ini 的 monitor_speed 一致，否则是乱码
  Serial.begin(115200);

  pinMode(PM25_LED_PIN, OUTPUT);
  digitalWrite(PM25_LED_PIN, LED_OFF_LEVEL);  // 先熄灭，别在启动时白白点亮
  pinMode(PM25_AN_PIN, INPUT);

  Serial.println();
  Serial.println("=== GP2Y1014AU 模块 PM2.5 测试 ===");
  Serial.print("  LED 驱动脚 = GPIO");
  Serial.print(PM25_LED_PIN);
  Serial.print("  (");
  Serial.print(LED_ON_LEVEL == HIGH ? "高电平点亮" : "低电平点亮");
  Serial.print(")    模拟输入脚 = GPIO");
  Serial.println(PM25_AN_PIN);
  Serial.println("  分压还原 x11   无尘基准 400mV   系数 0.20/mV");
  Serial.println("  预期：raw 只在 0~450 之间，吹烟后应明显上升");
  Serial.println();
}

// ================== 主循环 ==================
// 这里用 delay(1000) 是安全的 —— 本程序没有任何网络要服务，
// delay() 只在 WebServer 那种要频繁 handleClient() 的场景才是坑。
void loop() {
  int raw = 0;
  float voltageMv = 0;
  int pm25 = readPM25(raw, voltageMv);

  // 记录运行区间，方便判断「读数到底会不会动」
  static int minRaw = 4095, maxRaw = 0;
  if (raw < minRaw) minRaw = raw;
  if (raw > maxRaw) maxRaw = raw;

  Serial.print("raw=");
  Serial.print(raw);
  Serial.print("  Vo=");
  Serial.print(voltageMv, 0);
  Serial.print("mV  PM2.5=");
  Serial.print(pm25);
  Serial.print(" ug/m3   [min=");
  Serial.print(minRaw);
  Serial.print(" max=");
  Serial.print(maxRaw);
  Serial.println("]");

  // 每 10 秒给一句提示，免得看着一屏数字不知道该干嘛
  static int count = 0;
  if (++count % 10 == 0) {
    Serial.println("  ↑ 试试：在进气口附近点根蚊香 / 吹口气，看 raw 会不会涨");
    if (minRaw == maxRaw) {
      Serial.println("  ⚠ raw 一直没变过 —— 可能是 LED 极性反了，把 LED_ON_LEVEL 改成 ");
      Serial.println(LED_ON_LEVEL == HIGH ? "LOW 再烧一次" : "HIGH 再烧一次");
    }
  }

  delay(1000);
}
