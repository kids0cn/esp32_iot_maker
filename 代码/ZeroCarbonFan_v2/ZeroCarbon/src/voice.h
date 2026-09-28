#pragma once
/*
 * 语音识别模块 —— 启英泰伦 CI1302（串口 UART1 · 115200 · 8 字节协议帧）
 * ============================================
 * 和 light.h / pm25.h / dht11.h / relay.h 一个套路：常量 + voiceInit() + voicePoll()。
 * 但多了一个 **voiceTakeCmd()**，因为它和别的模块性质不同：
 *   传感器是「读数」—— 每秒读一次，读多少次都一样；
 *   语音是「事件」—— 来一条指令只该执行一次，处理完必须清掉。
 *   不取走就会每轮 loop() 都触发一次，风扇会被这条指令按着不放。
 *
 * ── 接线（详见 文档/接线与引脚.md 2.6）──────────────
 *   模块 5V   → 5V（走 DC-DC，别从 3.3V 取 —— 模块带功放）
 *   模块 GND  → GND
 *   模块 TX   → GPIO16（UART2 的 RX）
 *   模块 RX   → GPIO17（UART2 的 TX）
 *   115200、8N1
 *
 * ── 协议帧（8 字节）──────────────────────────
 *   A5 FA 00 <类型> <命令词> 00 <校验> FB
 *     类型 0x80 = 握手   0x81 = 识别结果   0x82 = 主机要求它播报
 *
 * ⚠️ 第 6 个字节（下标 6）**不是校验和，是厂商命令码** ——
 *    它是厂商协议生成平台分配的一个字节，**推不出算法**：
 *    整个 SDK 的串口这条路（user_msg_deal.c）里帧全是写死的字符串，
 *    压根没有算校验和的代码（crc16 只出现在蓝牙那套 app_ble/ 里，与串口无关）。
 *    所以只能**整帧逐字节比对**，别自作聪明改成「算校验和」，会认错指令。
 *    帧表逐字节抄自厂商 SDK 的 user_msg_deal.c（send_data[] / recv_data[]），
 *    **完整指令表（含同义说法、握手帧）见 文档/传感器/语音控制模块.md**。
 *
 * ── ⚠️ 上电握手必须应答（2026-09-27 实测）──────────
 *   模块上电后反复发 A5 FA 00 80 0A 00 21 FB，等我们回
 *   A5 FA 00 80 0A 00 22 FB。
 *   不回会怎样：它每 0.4 秒重发一次，而且校准期间会把波特率在
 *   115200×0.90 ~ ×1.10 之间来回试 —— **换挡那几帧收进来就是错位字节**
 *   （实测出现过 A5 FA 00 00 0A 00 61 FB，类型 0x00 不属于任何已知类型）。
 *   voicePoll() 里自动回，理由：能把这个 8 字节帧逐字节认全，
 *   本身就证明波特率是对的，所以这个 ACK 回得放心。
 *
 * ── ⚠️ voicePoll() 必须非阻塞 ──────────────────
 *   main 的 loop() 里要频繁调 server.handleClient()，**一次 delay() 网页就没响应**。
 *   本函数只把串口缓冲里现有的字节读完就返回 —— 一帧 8 字节 @115200 约 0.7ms，
 *   每轮最多几毫秒，无感。**不要在里面加等待**。
 *
 * ── 语音算「手动」还是「自动」──────────────────
 *   main 的约定：**执行动作 + 切到手动模式**。
 *   不切的话会重演原版固件那个坑 —— 语音刚关掉的风扇，下一轮自动判据
 *   又给开回来（CLAUDE.md 已知的坑第 5 条）。
 */

#include <Arduino.h>
#include <string.h>   // memcmp / memmove —— 组帧和查表要用

// ================== 常量 ==================
const int VOICE_RX_PIN   = 16;        // 模块的发送脚 → ESP32（UART2 的 RX）
const int VOICE_TX_PIN   = 17;        // ESP32（UART2 的 TX）→ 模块的接收脚
const unsigned long VOICE_BAUD = 115200;

// ================== 语音指令 ==================
// voiceTakeCmd() 的返回值。VC_NONE = 这次没有新指令。
enum VoiceCmd : int8_t {
  VC_NONE = -1,
  VC_WAKE,        // 你好小丹（唤醒词）—— 不控设备

  // ★ 下面这 8 条**都控设备**，所以**必须连着排** ——
  //   voiceCmdIsDevice() 用「范围判断」认它们，中间要是插进一条不控设备的，
  //   范围就会把它也圈进去（说话就白切一次模式）。加新指令时留意。
  VC_LIGHT_ON,    // 开灯       → IN2
  VC_LIGHT_OFF,   // 关灯       → IN2
  VC_FAN_ON,      // 开风扇     → IN1
  VC_FAN_OFF,     // 关闭风扇   → IN1
  VC_DEHUM_ON,    // 开抽湿机   → IN3
  VC_DEHUM_OFF,   // 关抽湿机   → IN3
  VC_AC_ON,       // 开空调     → IN4
  VC_AC_OFF,      // 关空调     → IN4

  VC_WELCOME,     // 欢迎语（模块上电时播）—— 不控设备
  VC_BYE,         // 休息语（退出唤醒时播）—— 不控设备
};

// 这条指令会不会动设备？—— main 用它决定要不要顺带切成「手动模式」。
// ★ 用范围判断而不是一串 `||`：2026-09-28 加空调 / 抽湿机那 4 条时就吃过亏 ——
//   `||` 写法漏改一处，症状是「说了话设备动了、但模式没切」，
//   下一轮自动逻辑又给改回去，看着像指令没生效。范围写法加指令不用改这里。
inline bool voiceCmdIsDevice(int8_t c) {
  return c >= VC_LIGHT_ON && c <= VC_AC_OFF;
}

// 帧表：模块 → ESP32（类型 0x81）。bytes 必须逐字节精确匹配，见文件头说明。
//
// ★ 一个动作可能有**几种说法**，它们发的是**同一帧**（固件 2026-09-27 更新后
//   同时收这几种叫法）：说「开风扇」和「打开风扇」，模块都发 81 02 00 23；
//   说「关闭风扇」和「关风扇」，都发 81 03 00 25。
//   所以表里的 name 用短的那个（网页上行宽有限），同义说法写在下面。
//   完整的命令词清单见 文档/传感器/语音控制模块.md。
struct VoiceFrame {
  uint8_t     bytes[8];
  int8_t      cmd;
  const char* name;
};

static const VoiceFrame VOICE_RX_TABLE[] = {
  { {0xA5,0xFA,0x00,0x81,0x01,0x00,0x21,0xFB}, VC_WAKE,      "你好小丹（唤醒词）" },
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x22,0xFB}, VC_LIGHT_ON,  "开灯"               },
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x23,0xFB}, VC_FAN_ON,    "开风扇"             },  // 也叫「打开风扇」
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x24,0xFB}, VC_LIGHT_OFF, "关灯"               },
  { {0xA5,0xFA,0x00,0x81,0x03,0x00,0x25,0xFB}, VC_FAN_OFF,   "关闭风扇"           },  // 也叫「关风扇」
  // ↓ 2026-09-28 厂商 readme 新增的 4 条，正好把 4 路设备凑齐
  { {0xA5,0xFA,0x00,0x81,0x03,0x00,0x28,0xFB}, VC_DEHUM_ON,  "开抽湿机"           },
  // ⚠️「关抽湿机」厂商给的是 81 02 00 25 —— **校验 25 和「关闭风扇」撞了**，
  //   而「开抽湿机」是 28，顺着往下排本该是 29。看着像厂商抄错了行。
  //   先按 readme 原样写上。真机说一句「关抽湿机」，串口要是打「未登记的帧」，
  //   把 hex 贴出来换掉这一行 —— **校验位算不出来，只能整帧逐字节比对**。
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x25,0xFB}, VC_DEHUM_OFF, "关抽湿机"           },
  { {0xA5,0xFA,0x00,0x81,0x03,0x00,0x26,0xFB}, VC_AC_ON,     "开空调"             },
  { {0xA5,0xFA,0x00,0x81,0x03,0x00,0x27,0xFB}, VC_AC_OFF,    "关空调"             },
  { {0xA5,0xFA,0x00,0x81,0x0A,0x00,0x2A,0xFB}, VC_WELCOME,   "欢迎语"             },
  { {0xA5,0xFA,0x00,0x81,0x0B,0x00,0x2B,0xFB}, VC_BYE,       "休息语"             },
};
static const int VOICE_RX_TABLE_LEN =
    sizeof(VOICE_RX_TABLE) / sizeof(VOICE_RX_TABLE[0]);

// 上电握手：请求 / 应答。只差最后一个校验字节。
static const uint8_t VOICE_SYNC_REQ[8] = {0xA5,0xFA,0x00,0x80,0x0A,0x00,0x21,0xFB};
static const uint8_t VOICE_SYNC_ACK[8] = {0xA5,0xFA,0x00,0x80,0x0A,0x00,0x22,0xFB};

// ================== 内部状态 ==================
// static：只在本编译单元可见。本模块只被 main.cpp include 一次，
// 但保持这个习惯 —— 头文件被多处 include 时不会撞符号。
static const int VOICE_RX_BUF = 64;   // 组帧缓冲（一帧才 8 字节，够宽松了）
static uint8_t   voiceRxBuf[VOICE_RX_BUF];
static int       voiceRxLen   = 0;
static int8_t    voicePending = VC_NONE;   // 攒着还没被取走的指令
// ★ 只留**一条**，不做队列 —— 这是有意的：
//   loop() 里 voicePoll() 和 voiceTakeCmd() 是紧挨着调的，两条指令之间
//   隔着零点几毫秒就被取走了，实际不会撞。万一真撞上（连说两条），
//   后一条覆盖前一条 —— 按「用户最后说的算」处理，比丢新指令合理。
static bool        voiceSynced  = false;   // 握过手没有（= 模块在线）
static const char* voiceLastCmd = "";      // 最近一条指令的名字（指向表里的字面量）

// 从缓冲开头丢掉 n 个字节
static inline void voiceDropBytes(int n) {
  if (n >= voiceRxLen) { voiceRxLen = 0; return; }
  memmove(voiceRxBuf, voiceRxBuf + n, voiceRxLen - n);
  voiceRxLen -= n;
}

// 处理一整帧
static inline void voiceHandleFrame(const uint8_t* f) {
  // ① 握手：认出请求就立刻回 ACK（见文件头「上电握手必须应答」）
  if (memcmp(f, VOICE_SYNC_REQ, 8) == 0) {
    Serial2.write(VOICE_SYNC_ACK, 8);
    voiceSynced = true;                 // 握手过了 = 模块在线
    Serial.println("  [语音] 收到握手，已回 ACK");
    return;
  }

  // ② 指令：整帧比对
  for (int i = 0; i < VOICE_RX_TABLE_LEN; i++) {
    if (memcmp(f, VOICE_RX_TABLE[i].bytes, 8) == 0) {
      voicePending = VOICE_RX_TABLE[i].cmd;
      voiceLastCmd = VOICE_RX_TABLE[i].name;   // 给网页显示「最近指令」用
      Serial.print("  [语音] 识别到：");
      Serial.println(VOICE_RX_TABLE[i].name);
      return;
    }
  }

  // ③ 不认识的帧：打出来，别闷着
  //    类型不在 0x80/0x81/0x82 里的，多半是模块换波特率时的错位字节；
  //    回了 ACK 之后这种帧应该就没了。留着这行日志是为了下次好认。
  Serial.print("  [语音] 未登记的帧");
  for (int i = 0; i < 8; i++) Serial.printf(" %02X", (unsigned)f[i]);
  Serial.println("   （对照 文档/传感器/语音控制模块.md 里的指令表）");
}

// 从缓冲里尽量抠出完整的 A5 FA … FB 帧
static inline void voiceParse() {
  for (;;) {
    // 1) 找帧头 A5 FA，前面的都是噪声（串口可能从一帧中间开始收）
    int start = -1;
    for (int i = 0; i + 1 < voiceRxLen; i++) {
      if (voiceRxBuf[i] == 0xA5 && voiceRxBuf[i + 1] == 0xFA) { start = i; break; }
    }
    if (start < 0) {
      // 没找到 A5 FA。但**末尾那个 0xA5 可能是下一帧的开头，不能丢** ——
      // 串口是按字节到的，一次 poll 很可能刚好只读到一帧的第一个字节。
      // 早先这里直接 rxLen = 0，那一帧就永远拼不起来了（真踩过）。
      if (voiceRxLen > 0 && voiceRxBuf[voiceRxLen - 1] == 0xA5) {
        voiceRxBuf[0] = 0xA5;      // 只留这一个字节，等后面的 FA 到
        voiceRxLen = 1;
      } else {
        voiceRxLen = 0;            // 连可能的帧头都没有，整段丢掉
      }
      return;
    }
    if (start > 0) voiceDropBytes(start);

    // 2) 还没收满一帧，等下一批字节
    if (voiceRxLen < 8) return;

    // 3) 帧尾不是 FB → 说明帧头是撞出来的，丢 1 字节重找
    if (voiceRxBuf[7] != 0xFB) { voiceDropBytes(1); continue; }

    // 4) 完整一帧
    voiceHandleFrame(voiceRxBuf);
    voiceDropBytes(8);
  }
}

// ================== 初始化：setup() 里调一次 ==================
inline void voiceInit() {
  // 用 Serial2（UART2）。**别动 Serial** —— 那是 USB 串口和日志，
  // 占了就看不到输出了。
  Serial2.begin(VOICE_BAUD, SERIAL_8N1, VOICE_RX_PIN, VOICE_TX_PIN);
}

// ================== 轮询：loop() 里每轮调，非阻塞 ==================
inline void voicePoll() {
  while (Serial2.available()) {
    if (voiceRxLen >= VOICE_RX_BUF) voiceRxLen = 0;   // 溢出兜底
    voiceRxBuf[voiceRxLen++] = (uint8_t)Serial2.read();
  }
  if (voiceRxLen > 0) voiceParse();
}

// ================== 取指令：取走即清空 ==================
// 返回 VC_NONE 表示这次没有新指令。
// ★ 语义是「取走」—— 调一次就把攒的指令交出去并清掉，
//   所以调用方拿到一条就处理一条，不会重复触发。
inline int8_t voiceTakeCmd() {
  int8_t c = voicePending;
  voicePending = VC_NONE;
  return c;
}

// ================== 状态查询：给 main 打日志 / 填网页用 ==================
// 模块上电后跟我们握手过没有（= 它在线）。没握手多半是没接、没供电、
// 或者两根信号线接反了 —— 网页上显示成「未握手」就是在提示这个。
inline bool voiceIsSynced() { return voiceSynced; }

// 最近一条指令的名字（如「开风扇」）。没收到过返回空串。
inline const char* voiceCmdName() { return voiceLastCmd; }
