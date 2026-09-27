#pragma once
#ifdef __cplusplus
extern "C" {
#endif
#include "stdint.h"


#define UART1_GPIO_TXD           (GPIO_NUM_35)
#define UART1_GPIO_RXD           (GPIO_NUM_36)

#define RX1_BUF_SIZE             (1024)

//播报词 Active broadcast content
#define This_red 0x5F    
#define This_blue 0x60
#define This_green 0x61
#define This_yellow 0x62
#define Recognize_yellow 0x63
#define Recognize_green 0x64
#define Recognize_blue 0x65
#define Recognize_red 0x66
#define init 0x67     

void Uart1_Init(void);
int Uart1_Send_Byte(uint8_t data);
int Uart1_Send_Data(uint8_t* data, uint16_t len);


uint16_t Uart1_Available(void);
uint8_t Uart1_Read(void);
void Uart1_Clean_Buffer(void);


#ifdef __cplusplus
}
#endif
