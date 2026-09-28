// voice.h —— 语音模块 CI1302（串口 UART2 · 115200 · 8 字节协议帧）
// 注意：① 上电必须回握手 ACK，否则模块每 0.4 秒重发、还会来回试波特率产生错位字节；
//       ② 第 6 个字节是厂商命令码、**不是校验和**，只能整帧逐字节比对。

#pragma once

#include <Arduino.h>
#include <string.h>

const int VOICE_RX_PIN   = 16;
const int VOICE_TX_PIN   = 17;
const unsigned long VOICE_BAUD = 115200;

enum VoiceCmd : int8_t {
  VC_NONE = -1,
  VC_WAKE,

  VC_LIGHT_ON,
  VC_LIGHT_OFF,
  VC_FAN_ON,
  VC_FAN_OFF,
  VC_DEHUM_ON,
  VC_DEHUM_OFF,
  VC_AC_ON,
  VC_AC_OFF,

  VC_WELCOME,
  VC_BYE,
};

inline bool voiceCmdIsDevice(int8_t c) {
  return c >= VC_LIGHT_ON && c <= VC_AC_OFF;
}

struct VoiceFrame {
  uint8_t     bytes[8];
  int8_t      cmd;
  const char* name;
};

static const VoiceFrame VOICE_RX_TABLE[] = {
  { {0xA5,0xFA,0x00,0x81,0x01,0x00,0x21,0xFB}, VC_WAKE,      "你好小丹（唤醒词）" },
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x22,0xFB}, VC_LIGHT_ON,  "开灯"               },
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x23,0xFB}, VC_FAN_ON,    "开风扇"             },
  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x24,0xFB}, VC_LIGHT_OFF, "关灯"               },
  { {0xA5,0xFA,0x00,0x81,0x03,0x00,0x25,0xFB}, VC_FAN_OFF,   "关闭风扇"           },

  { {0xA5,0xFA,0x00,0x81,0x03,0x00,0x28,0xFB}, VC_DEHUM_ON,  "开抽湿机"           },

  { {0xA5,0xFA,0x00,0x81,0x02,0x00,0x25,0xFB}, VC_DEHUM_OFF, "关抽湿机"           },
  { {0xA5,0xFA,0x00,0x81,0x03,0x00,0x26,0xFB}, VC_AC_ON,     "开空调"             },
  { {0xA5,0xFA,0x00,0x81,0x03,0x00,0x27,0xFB}, VC_AC_OFF,    "关空调"             },
  { {0xA5,0xFA,0x00,0x81,0x0A,0x00,0x2A,0xFB}, VC_WELCOME,   "欢迎语"             },
  { {0xA5,0xFA,0x00,0x81,0x0B,0x00,0x2B,0xFB}, VC_BYE,       "休息语"             },
};
static const int VOICE_RX_TABLE_LEN =
    sizeof(VOICE_RX_TABLE) / sizeof(VOICE_RX_TABLE[0]);

static const uint8_t VOICE_SYNC_REQ[8] = {0xA5,0xFA,0x00,0x80,0x0A,0x00,0x21,0xFB};
static const uint8_t VOICE_SYNC_ACK[8] = {0xA5,0xFA,0x00,0x80,0x0A,0x00,0x22,0xFB};

static const int VOICE_RX_BUF = 64;
static uint8_t   voiceRxBuf[VOICE_RX_BUF];
static int       voiceRxLen   = 0;
static int8_t    voicePending = VC_NONE;

static bool        voiceSynced  = false;
static const char* voiceLastCmd = "";

static inline void voiceDropBytes(int n) {
  if (n >= voiceRxLen) { voiceRxLen = 0; return; }
  memmove(voiceRxBuf, voiceRxBuf + n, voiceRxLen - n);
  voiceRxLen -= n;
}

static inline void voiceHandleFrame(const uint8_t* f) {
  if (memcmp(f, VOICE_SYNC_REQ, 8) == 0) {
    Serial2.write(VOICE_SYNC_ACK, 8);
    voiceSynced = true;
    Serial.println("  [语音] 收到握手，已回 ACK");
    return;
  }

  for (int i = 0; i < VOICE_RX_TABLE_LEN; i++) {
    if (memcmp(f, VOICE_RX_TABLE[i].bytes, 8) == 0) {
      voicePending = VOICE_RX_TABLE[i].cmd;
      voiceLastCmd = VOICE_RX_TABLE[i].name;
      Serial.print("  [语音] 识别到：");
      Serial.println(VOICE_RX_TABLE[i].name);
      return;
    }
  }

  Serial.print("  [语音] 未登记的帧");
  for (int i = 0; i < 8; i++) Serial.printf(" %02X", (unsigned)f[i]);
  Serial.println("   （对照 文档/传感器/语音控制模块.md 里的指令表）");
}

static inline void voiceParse() {
  for (;;) {
    int start = -1;
    for (int i = 0; i + 1 < voiceRxLen; i++) {
      if (voiceRxBuf[i] == 0xA5 && voiceRxBuf[i + 1] == 0xFA) { start = i; break; }
    }
    if (start < 0) {
      if (voiceRxLen > 0 && voiceRxBuf[voiceRxLen - 1] == 0xA5) {
        voiceRxBuf[0] = 0xA5;
        voiceRxLen = 1;
      } else {
        voiceRxLen = 0;
      }
      return;
    }
    if (start > 0) voiceDropBytes(start);

    if (voiceRxLen < 8) return;

    if (voiceRxBuf[7] != 0xFB) { voiceDropBytes(1); continue; }

    voiceHandleFrame(voiceRxBuf);
    voiceDropBytes(8);
  }
}

inline void voiceInit() {
  Serial2.begin(VOICE_BAUD, SERIAL_8N1, VOICE_RX_PIN, VOICE_TX_PIN);
}

inline void voicePoll() {
  while (Serial2.available()) {
    if (voiceRxLen >= VOICE_RX_BUF) voiceRxLen = 0;
    voiceRxBuf[voiceRxLen++] = (uint8_t)Serial2.read();
  }
  if (voiceRxLen > 0) voiceParse();
}

inline int8_t voiceTakeCmd() {
  int8_t c = voicePending;
  voicePending = VC_NONE;
  return c;
}

inline bool voiceIsSynced() { return voiceSynced; }

inline const char* voiceCmdName() { return voiceLastCmd; }
