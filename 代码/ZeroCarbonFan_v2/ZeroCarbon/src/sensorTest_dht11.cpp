/*
 * DHT11 温湿度传感器测试（YL-47 模块 · 3 线）
 * ============================================
 * 独立测试程序：只读 DHT11、往串口打印，不开 WiFi。
 * 目的：确认「接线对不对、模块活没活、读数合不合理」。
 *
 * ── 接线：3 根线 ──────────────────────────────
 *   YL-47 模块        ESP32
 *   ─────────────────────────────────────────
 *   VCC  (1 脚)  ───→  3.3V        ⚠️ 见下方「为什么接 3.3V」
 *   DATA (2 脚)  ───→  GPIO4       ⚠️ 见下方「引脚选择」
 *   GND  (3 脚)  ───→  GND
 *
 *   **上拉电阻不用自己加** —— YL-47 模块板上已经有 R1 4.7k 了
 *   （手册建议 5k，4.7k 符合）。模块上还有一颗电源指示灯 D1，
 *   **D1 亮 = 模块通电**，排查时先看它。
 *
 * ── 为什么接 3.3V，不接 5V ────────────────────
 *   手册写 DHT11 供电 3~5.5V，两种都行。但 DATA 是靠**上拉到 VCC** 的：
 *   接 5V 的话 DATA 高电平就是 5V，而 **ESP32 的 GPIO 不是 5V 容限**，
 *   长期接有打坏引脚的风险。接 3.3V 则 DATA 摆幅 0~3.3V，安全。
 *   （和光敏模块同一个道理，和 PM2.5 转接板相反 —— 那块必须 5V。）
 *
 * ── 引脚选择：GPIO4 ───────────────────────────
 *   DHT 是**单总线数字信号**，不是模拟量，所以不挑 ADC，普通 GPIO 就行。
 *   GPIO4 空闲、非 strapping（0/2/5/12/15）、非 Flash（6~11）、非串口（1/3）。
 *   现有占用：14（PM2.5 ILED）、32（PM2.5 AO）、35（光敏 AO）。
 *
 * ── ⚠️ 两条硬约束（手册要求，不遵守读出来就是乱的）──
 *   ① **上电后必须等 1 秒**才能发指令 ——
 *      手册原文：「传感器上电后，要等待 1s 以越过不稳定状态」。
 *      所以 setup() 里有 delay(1000)。
 *   ② **采样周期不能太快** —— DHT11 最快约 1 秒一次（有的资料说 2 秒）。
 *      本程序用 2 秒一次，留足余量。
 *
 * ── 关于这个程序为什么不用库 ──────────────────
 *   常见的 DHT 库（Adafruit DHT / DHTesp）要往 platformio.ini 里加 lib_deps。
 *   这里手写位操作实现，**不用装任何库**，也顺便能看清 DHT11 的时序长什么样。
 *   （代价：时序敏感，偶发读失败是正常的 —— 程序里有重试和统计。）
 *
 * ── DHT11 的规格（厂商手册）──────────────────
 *   湿度：20–90 %RH，±5 %RH
 *   温度：0–50 °C，±2 °C
 *   分辨力：1（整数，没有小数位 —— 这是 DHT11 和 DHT22 的主要差别）
 *
 * ── 怎么读串口输出 ────────────────────────────
 *   成功： OK   温度=25 C   湿度=48 %RH    (校验和 0xXX)
 *   失败： FAIL 读不到（连续失败 3 次会给出排查提示）
 *   结尾会打一行统计：成功 x 次 / 失败 y 次
 */

#include <Arduino.h>

// ================== 引脚 ==================
const int DHT_PIN = 4;

// ================== 采样周期 ==================
const unsigned long DHT_READ_MS = 2000;   // 2 秒读一次（手册要求 ≥1 秒）

// ================== 时序常量（依据 DHT11 手册）==================
const int START_LOW_MS      = 20;     // 起始信号：拉低 ≥18ms（这里取 20ms 留余量）
const int BIT_SAMPLE_US     = 35;     // 每位在高电平期间采样：35µs 时还高 = 1，已低 = 0
const int WAIT_TIMEOUT_US   = 200;    // 等电平翻转的超时，防止死等

// ================== 统计 ==================
int okCount = 0, failCount = 0, consecutiveFail = 0;

// 等引脚变成指定电平，超时返回 false（防止 while 死循环卡住）
bool waitLevel(int level, unsigned long timeoutUs) {
  unsigned long t0 = micros();
  while (digitalRead(DHT_PIN) != level) {
    if (micros() - t0 > timeoutUs) return false;
  }
  return true;
}

// ================== 读一次 ==================
// 成功返回 true，并把整数温湿度写进 tempOut / humiOut
//
// DHT11 的通信过程（单总线，一根线双向）：
//   ① 主机把线拉低 ≥18ms，再放开        —— 告诉它「我要读数据了」
//   ② DHT11 应答：拉低 80µs，再拉高 80µs —— 「我准备好了」
//   ③ DHT11 连续发 40 位（5 字节）：
//        湿整 湿小 温整 温小 校验和
//      每一位都是「低 50µs + 高 X µs」，
//        X ≈ 26µs → 这位是 0
//        X ≈ 70µs → 这位是 1
//      所以只要在「低电平结束后 35µs」去读引脚：
//        还是高 → 那位是 1；已经变低 → 那位是 0
bool dht11Read(int8_t& tempOut, int8_t& humiOut) {
  uint8_t data[5] = {0};

  // ① 起始信号
  pinMode(DHT_PIN, OUTPUT);
  digitalWrite(DHT_PIN, LOW);
  delay(START_LOW_MS);
  pinMode(DHT_PIN, INPUT);          // 放开，模块上的 4.7k 上拉把线拉高

  // ② 等 DHT11 应答：先拉低 80µs，再拉高 80µs
  if (!waitLevel(LOW,  WAIT_TIMEOUT_US)) return false;   // 等它拉低
  if (!waitLevel(HIGH, WAIT_TIMEOUT_US)) return false;   // 等它拉高

  // ③ 读 40 位
  for (int i = 0; i < 40; i++) {
    if (!waitLevel(LOW,  WAIT_TIMEOUT_US)) return false; // 每位的 50µs 低电平
    if (!waitLevel(HIGH, WAIT_TIMEOUT_US)) return false; // 等低电平结束
    delayMicroseconds(BIT_SAMPLE_US);                    // 35µs 后采样
    data[i >> 3] <<= 1;
    if (digitalRead(DHT_PIN) == HIGH) data[i >> 3] |= 1;
  }

  // ④ 校验：第 5 字节 = 前 4 字节之和的低 8 位
  uint8_t sum = data[0] + data[1] + data[2] + data[3];
  if (data[4] != sum) return false;

  // DHT11：data[0]=湿度整数 data[1]=湿度小数(恒0)
  //        data[2]=温度整数 data[3]=温度小数(恒0)
  humiOut = (int8_t)data[0];
  tempOut = (int8_t)data[2];
  return true;
}

// ================== 初始化 ==================
void setup() {
  Serial.begin(115200);

  Serial.println();
  Serial.println("=== DHT11 温湿度传感器测试（YL-47 模块）===");
  Serial.println("  接线：VCC→3.3V（不是5V）  DATA→GPIO4  GND→GND");
  Serial.print("  数据脚 = GPIO");
  Serial.println(DHT_PIN);
  Serial.println("  模块上已有 4.7k 上拉 + 电源灯 D1，D1 亮 = 通电");
  Serial.println();
  Serial.println("  ⚠️ 手册要求：上电后要等 1 秒才能读，正在等…");

  // 手册明文要求：上电后等 1s 越过不稳定状态
  delay(1000);

  Serial.println("  开始采样（每 2 秒一次）");
  Serial.println();
}

// ================== 主循环 ==================
// 这里用 delay() 没问题 —— 本程序没有任何网络要服务，
// delay() 只在 WebServer 那种要频繁 handleClient() 的场景才是坑。
void loop() {
  int8_t t = 0, h = 0;
  bool ok = dht11Read(t, h);
  // 失败时重试一次 —— DHT11 时序敏感，偶发失败很正常
  if (!ok) {
    delay(100);
    ok = dht11Read(t, h);
  }

  if (ok) {
    okCount++;
    consecutiveFail = 0;
    Serial.print("OK   温度=");
    Serial.print(t);
    Serial.print(" C   湿度=");
    Serial.print(h);
    Serial.print(" %RH     [成功 ");
    Serial.print(okCount);
    Serial.print(" / 失败 ");
    Serial.print(failCount);
    Serial.println("]");
  } else {
    failCount++;
    consecutiveFail++;
    Serial.print("FAIL 读不到数据     [成功 ");
    Serial.print(okCount);
    Serial.print(" / 失败 ");
    Serial.print(failCount);
    Serial.println("]");

    if (consecutiveFail == 3) {
      Serial.println("  ── 连续 3 次读失败，按这个顺序查 ──────────────");
      Serial.println("   1. 模块上的电源灯 D1 亮吗？不亮 → VCC/GND 没接好");
      Serial.println("   2. VCC 接的是 3.3V 吗？（手册 3~5.5V 都行，但别飞线到 5V）");
      Serial.println("   3. DATA 真的插在 GPIO4 上吗？换根杜邦线试试");
      Serial.println("   4. GND 和 ESP32 共地了吗？");
      Serial.println("   5. 上电后等够 1 秒了吗？（手册要求，本程序已自动等）");
      Serial.println("   ──────────────────────────────────────────");
    }
  }

  // 每 20 次给一行统计，看整体成功率
  static int n = 0;
  if (++n % 20 == 0) {
    Serial.print("  ── 统计：" );
    Serial.print(okCount);
    Serial.print(" 成功 / ");
    Serial.print(failCount);
    Serial.print(" 失败   成功率 ");
    Serial.print(okCount * 100 / (okCount + failCount));
    Serial.println("%");
    Serial.println("     成功率低但非零 → 多半是杜邦线接触不良或线太长");
    Serial.println();
  }

  delay(DHT_READ_MS);
}
