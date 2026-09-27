/*
 * 语音模块测试 —— 每条语音指令打到串口控制台
 * ============================================
 * 目的：**在并进主程序之前，先确认语音模块接对了、命令词收得到。**
 *
 * 为什么不直接上主程序：主程序一跑就开热点 + 自动控制，串口刷的是网页和传感器
 * 的日志，混在里面分不清「没收到指令」是接线问题还是解析问题。先单独测最省事。
 *
 * ── 接线（完整接线见 文档/接线与引脚.md）──────────
 *   语音模块          接到 ESP32
 *   5V      ───────→  5V            ⚠️ 走 DC-DC，别从 3.3V 取（模块带功放，电流不小）
 *   GND     ───────→  GND           ⚠️ 必须和 ESP32 共地
 *   信号脚① ───────→  GPIO 16       程序里的 RX：模块 → ESP32
 *   信号脚② ───────→  GPIO 17       程序里的 TX：ESP32 → 模块
 *   波特率 115200、8N1 —— 和固件 UART_PROTOCOL_BAUDRATE 一致
 *
 * ⚠️ 丝印两个信号脚都写着 TX（TX1 / TX2），**光看丝印分不出谁发谁收**。
 *    判定办法（一次只接一根，避免两个推挽输出对撞）：
 *      ① 先只把信号脚①接到 GPIO16，信号脚②空着，上电
 *      ② 对模块说「你好小丹」
 *         · 控制台有打印 → ① 就是模块的发送脚，把②接到 GPIO17，接线完成
 *         · 一个字都没有 → 断电，把两根信号线对调再试
 *
 * ── 它会做什么 ──────────────────────────────────
 *   1. 每收到一帧，**先原样打 hex**，再对照指令表打中文指令名
 *   2. 表里没有的帧照样打 hex + 各字段，不静默丢弃（换命令词后还看得见东西）
 *   3. 收到**握手帧**就自动回 ACK —— 不回的话模块会一直重发（见下面）
 *   4. 控制台按 1~7 反向发一帧让模块念一句话 —— 验的是 ESP32→模块 那根线
 *
 * ── 上电握手（2026-09-27 实测到的，很重要）──────────
 *   模块上电后会**反复**发这一帧等我们回话：
 *       收：A5 FA 00 80 0A 00 21 FB   ← 波特率同步请求
 *       发：A5 FA 00 80 0A 00 22 FB   ← 我们的 ACK（只差最后的校验字节）
 *   一直不回的话，模块会**每 0.4 秒重发一次**，而且它在校准期间会
 *   把波特率在 115200×0.90 ~ 115200×1.10 之间来回试 —— 换挡那几帧
 *   收进来就是错位的乱码（实测出现过 `A5 FA 00 00 0A 00 61 FB`）。
 *   回了 ACK，模块才锁定波特率、停止重试，才会走到发指令那一步。
 *
 *   **为什么敢自动回**：ACK 的意思是「你这个波特率我收对了」——
 *   而我们能逐字节认出这个 8 字节帧，本身就证明波特率是对的。
 *   （ACK 格式出处：SDK `user_msg_deal.c` 里
 *     `memcmp(p_data, "\xA5\xFA\x00\x80\x0A\x00\x22\xFB", length)`，
 *     和你那份 xlsx「握手协议 · 芯片接收」那一行完全一致。）
 *
 * ── 怎么判断 ────────────────────────────────────
 *   · 收到握手帧 + 打出「已回 ACK」      ✓ 通了，等它别再重发
 *   · 说「你好小丹」「开风扇」→ 打出中文指令名   ✓ 握手过了，指令也通
 *   · 只有 hex、标着「未登记」 → 协议对不上，把 hex 贴出来对表
 *   · 一个字都没有             → 接线 / 波特率 / 收发接反了（按上面①②③重来）
 *
 * ── 协议出处（都是厂商代码里逐字节写死的，不是我推的）──
 *   文档/传感器/语音控制模块/命令词播报词协议列表V3_中文模板.xlsx  ← 你那份表
 *   文档/传感器/语音控制模块/语音芯片sdk/CI13XX_SDK_.../.../user_msg_deal.c
 *     里面的 send_data[]（模块→主机）、recv_data[]（主机→模块）、
 *     以及波特率 ACK 的 memcmp
 *   固件配置 user_config.h：UART_PROTOCOL_NUMBER = HAL_UART1_BASE、115200
 */

#include <Arduino.h>
#include <string.h>   // memcmp / memmove（组帧和查表要用）

// =============== 接线（改这里要同步改 文档/接线与引脚.md）===============
const int VOICE_RX_PIN       = 16;      // 模块的发送脚 → ESP32 GPIO16（UART2 的 RX）
const int VOICE_TX_PIN       = 17;      // ESP32 GPIO17（UART2 的 TX）→ 模块的接收脚
const unsigned long VOICE_BAUD = 115200; // 和固件里 UART_PROTOCOL_BAUDRATE 一致

// =============== 指令表 ===============
// 帧格式 8 字节：A5 FA 00 <类型> <命令词> 00 <校验> FB
//   类型 0x81 = 模块发给主机（识别到命令词）
//   类型 0x82 = 主机发给模块（让它播一句）
//
// ⚠️ 第 7 字节的「校验」**不是**前 6 字节的累加和 —— 开风扇 / 关灯 / 关闭风扇
//    这三条对不上（按字节累加是 0x22 / 0x22 / 0x23，实际发的是 0x23 / 0x24 / 0x25）。
//    所以这里**只能整帧逐字节比对**，别改成「算校验和」，那样会认错指令。
struct VoiceFrame {
  uint8_t bytes[8];
  const char* name;
};

// 模块 → ESP32（识别到命令词 / 欢迎 / 休息）
const VoiceFrame RX_TABLE[] = {
  { {0xA5,0xFA,0x00,0x81,0x01,0x00,0x21,0xFB}, "你好小丹（唤醒词）" },
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x22,0xFB}, "开灯"                 },
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x23,0xFB}, "开风扇"               },
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x24,0xFB}, "关灯"                 },
  { {0xA5,0xFA,0x00,0x81,0x03,0x00,0x25,0xFB}, "关闭风扇"             },
  { {0xA5,0xFA,0x00,0x81,0x0A,0x00,0x2A,0xFB}, "欢迎语（上电）"        },
  { {0xA5,0xFA,0x00,0x81,0x0B,0x00,0x2B,0xFB}, "休息语（退出唤醒）"     },
};
const int RX_TABLE_LEN = sizeof(RX_TABLE) / sizeof(RX_TABLE[0]);

// ESP32 → 模块（让模块播对应播报语句），控制台按 1~7 发
const VoiceFrame TX_TABLE[] = {
  { {0xA5,0xFA,0x00,0x82,0x01,0x00,0x21,0xFB}, "1 → 播报「我在」"          },
  { {0xA5,0xFA,0x00,0x82,0x02,0x00,0x22,0xFB}, "2 → 播报「好的，灯已打开」" },
  { {0xA5,0xFA,0x00,0x82,0x02,0x00,0x23,0xFB}, "3 → 播报「好的，风扇已打开」"},
  { {0xA5,0xFA,0x00,0x82,0x02,0x00,0x24,0xFB}, "4 → 播报「好的，灯关闭开」" },
  { {0xA5,0xFA,0x00,0x82,0x03,0x00,0x25,0xFB}, "5 → 播报「好的，风扇已关闭」"},
  { {0xA5,0xFA,0x00,0x82,0x0A,0x00,0x2B,0xFB}, "6 → 播报欢迎语"            },
  { {0xA5,0xFA,0x00,0x82,0x0B,0x00,0x2C,0xFB}, "7 → 播报休息语"            },
};
const int TX_TABLE_LEN = sizeof(TX_TABLE) / sizeof(TX_TABLE[0]);

// ── 上电握手（波特率同步）────────────────────────
// 模块每隔一小段时间发 SYNC_REQ 等我们回话；回了 SYNC_ACK 它才锁定波特率、
// 停止重试。两帧都只差最后一个校验字节。
// 出处：SDK user_msg_deal.c 里 defined_send_baudrate_sync_req() 发前者、
//       com_msg_process() 里 memcmp 认后者。和 xlsx「握手协议」两行一致。
const uint8_t SYNC_REQ[8] = {0xA5,0xFA,0x00,0x80,0x0A,0x00,0x21,0xFB};
const uint8_t SYNC_ACK[8] = {0xA5,0xFA,0x00,0x80,0x0A,0x00,0x22,0xFB};

// 已知的「类型」字节（第 4 字节）。认不出的类型多半是换波特率期间的错位字节。
bool isKnownType(uint8_t t) {
  return t == 0x80 || t == 0x81 || t == 0x82;
}

// =============== 接收缓冲与组帧 ===============
const int RX_BUF_SIZE = 64;
uint8_t rxBuf[RX_BUF_SIZE];
int rxLen = 0;

bool everReceived = false;          // 收到过至少一帧了吗（决定要不要刷「没数据」提示）
unsigned long lastHintMs = 0;

void printHex(const uint8_t* b, int n) {
  for (int i = 0; i < n; i++) {
    if (i) Serial.print(' ');
    Serial.printf("%02X", (unsigned)b[i]);
  }
}

// 从缓冲区开头丢掉 n 个字节
void dropBytes(int n) {
  if (n >= rxLen) { rxLen = 0; return; }
  memmove(rxBuf, rxBuf + n, rxLen - n);
  rxLen -= n;
}

// 处理一整帧：先原样打出来，再查表打中文
void handleFrame(const uint8_t* f) {
  everReceived = true;

  // 上电秒数（本地时间无关，这里只是「开机第几秒」），毫秒补零到 3 位
  Serial.printf("[%lu.%03lus] 收到 ", millis() / 1000, millis() % 1000);
  printHex(f, 8);

  for (int i = 0; i < RX_TABLE_LEN; i++) {
    if (memcmp(f, RX_TABLE[i].bytes, 8) == 0) {
      Serial.print("   → 指令：");
      Serial.println(RX_TABLE[i].name);
      return;
    }
  }

  // 握手帧：认出来就立刻回 ACK（我们能逐字节认出它，本身就是「波特率对了」的证明）
  if (memcmp(f, SYNC_REQ, 8) == 0) {
    Serial.println("   → 握手：波特率同步请求");
    Serial2.write(SYNC_ACK, 8);
    Serial.println("     已回 ACK（A5 FA 00 80 0A 00 22 FB）"
                   " —— 模块收到后锁定波特率、不再重发");
    return;
  }

  // 表里没有：把字段拆开打，方便对照厂商的表查
  Serial.print("   → 未登记的帧  类型=0x");
  Serial.print(f[3], HEX);
  Serial.print("  命令词=0x");
  Serial.print(f[4], HEX);
  Serial.print("  校验=0x");
  Serial.print(f[6], HEX);
  if (!isKnownType(f[3])) {
    // 类型不认识 —— 多半不是真帧，而是换波特率期间错位的字节
    Serial.println();
    Serial.println("     类型字节不在 0x80/0x81/0x82 之内，**多半是错位的字节**：");
    Serial.println("     模块在校准波特率时会在 115200×0.90 ~ ×1.10 之间来回试，");
    Serial.println("     换挡那几帧收进来就是乱的。回了 ACK 之后这种帧应该就没了。");
  } else {
    Serial.println("   （把上面这行 hex 对着 xlsx 的「发送协议」列核一下）");
  }
}

// 从缓冲区里尽量抠出完整的 A5 FA … FB 帧
void parseBuffer() {
  for (;;) {
    // 1) 找帧头 A5 FA，前面的都是噪声（串口可能从一帧中间开始收）
    int start = -1;
    for (int i = 0; i + 1 < rxLen; i++) {
      if (rxBuf[i] == 0xA5 && rxBuf[i + 1] == 0xFA) { start = i; break; }
    }
    if (start < 0) {
      // 没找到 A5 FA。但**末尾那个 0xA5 可能是下一帧的开头，不能丢** ——
      // 串口是按字节到的，一次 poll 很可能刚好只读到一帧的第一个字节，
      // 直接清空的话那一帧就永远拼不起来了（2026-09-27 自测时踩到）。
      if (rxLen > 0 && rxBuf[rxLen - 1] == 0xA5) {
        rxBuf[0] = 0xA5;      // 只留这一个字节，等后面的 FA 到
        rxLen = 1;
      } else {
        rxLen = 0;            // 连可能的帧头都没有，整段丢掉
      }
      return;
    }
    if (start > 0) { dropBytes(start); }

    // 2) 还没收满一帧，等下一批字节
    if (rxLen < 8) return;

    // 3) 帧尾不是 FB → 说明帧头是撞出来的，丢 1 字节重找
    if (rxBuf[7] != 0xFB) { dropBytes(1); continue; }

    // 4) 完整一帧
    handleFrame(rxBuf);
    dropBytes(8);
  }
}

// 按键 1~7 → 发一帧给模块，让它念一句（验反方向那根线）
void sendByKey(char key) {
  int idx = key - '1';
  if (idx < 0 || idx >= TX_TABLE_LEN) return;
  Serial2.write(TX_TABLE[idx].bytes, 8);
  Serial.print("  已发送 → ");
  printHex(TX_TABLE[idx].bytes, 8);
  Serial.print("   ");
  Serial.println(TX_TABLE[idx].name);
}

void printHelp() {
  Serial.println();
  Serial.println("── 上电握手（程序会自动做，不用按键）──────────");
  Serial.print("  模块发 ");
  printHex(SYNC_REQ, 8);
  Serial.println("  ← 波特率同步请求");
  Serial.print("  我们回 ");
  printHex(SYNC_ACK, 8);
  Serial.println("  ← ACK（收到请求就自动回）");
  Serial.println("  不回的话模块每 0.4 秒重发一次，还会在 115200×0.90~×1.10 之间");
  Serial.println("  来回试波特率 —— 换挡那几帧收进来就是错位的乱码。");
  Serial.println();
  Serial.println("── 控制台按键 ─────────────────────────────");
  Serial.println("  1~7  发一帧给模块，让它播一句（验 ESP32→模块 那根线）");
  Serial.println("  h    再打一遍这份帮助");
  Serial.println("  收指令不用按键 —— 对着模块说话就行。");
  Serial.println();
}

void setup() {
  Serial.begin(115200);
  delay(200);

  Serial2.begin(VOICE_BAUD, SERIAL_8N1, VOICE_RX_PIN, VOICE_TX_PIN);

  Serial.println();
  Serial.println("=== 语音模块测试 ===");
  Serial.print("  接线：模块5V→5V  GND→GND  信号①→GPIO");
  Serial.print(VOICE_RX_PIN);
  Serial.print("  信号②→GPIO");
  Serial.print(VOICE_TX_PIN);
  Serial.println("   波特率 115200");
  Serial.println("  ⚠️ 丝印两个信号脚都叫 TX —— 先只接①，说了话有打印再接②");
  Serial.println();
  Serial.println("  上电先看握手：模块会发 A5 FA 00 80 0A 00 21 FB，本程序收到就自动回");
  Serial.println("  ACK（… 22 FB）。握手过了、它不再刷屏，再对模块说话测指令。");
  Serial.println();
  Serial.println("  认识的指令：");
  for (int i = 0; i < RX_TABLE_LEN; i++) {
    Serial.print("    ");
    printHex(RX_TABLE[i].bytes, 8);
    Serial.print("  ");
    Serial.println(RX_TABLE[i].name);
  }
  printHelp();

  lastHintMs = millis();
}

void loop() {
  // ── 收：把串口来的字节攒起来组帧 ──
  while (Serial2.available()) {
    if (rxLen >= RX_BUF_SIZE) rxLen = 0;   // 溢出兜底，别把缓冲撑爆
    rxBuf[rxLen++] = (uint8_t)Serial2.read();
  }
  if (rxLen > 0) parseBuffer();

  // ── 发：控制台按键 ──
  while (Serial.available()) {
    char c = (char)Serial.read();
    if (c == 'h' || c == 'H') printHelp();
    else if (c >= '1' && c <= '7') sendByKey(c);
    else if (c != '\n' && c != '\r' && c != ' ') {
      Serial.print("  未知按键 '");
      Serial.print(c);
      Serial.println("' —— 按 h 看帮助");
    }
  }

  // ── 一直没数据就提醒一次，别让人干瞪眼 ──
  if (!everReceived && millis() - lastHintMs > 10000) {
    lastHintMs = millis();
    Serial.println("  …… 还没收到任何数据：检查 5V/GND 是否接好、"
                   "波特率是不是 115200、或者两根信号线接反了（断电对调再试）");
  }
}
