#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "stdint.h"
#include "esp_err.h"

// I2C主机相关参数配置 I2C master parameters
#define I2C_MASTER_GPIO_SCL         37
#define I2C_MASTER_GPIO_SDA         38
#define I2C_MASTER_NUM              0
#define I2C_MASTER_FREQ_HZ          400000
#define I2C_MASTER_TX_BUF_DISABLE   0
#define I2C_MASTER_RX_BUF_DISABLE   0
#define I2C_MASTER_TIMEOUT_MS       1000

#define VoicceADDR    0x2b
#define Write_register 0x03
#define Read_register 0x64
 
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



void I2C_Master_Init(void);
void I2C_Master_Delete(void);

esp_err_t I2C_Master_Read(uint8_t addr, uint8_t reg, uint16_t len, uint8_t* data);
uint8_t I2C_Master_Read_Byte(uint8_t addr, uint8_t reg);

esp_err_t I2C_Master_Write(uint8_t addr, uint8_t reg, uint16_t len, uint8_t* data);
esp_err_t I2C_Master_Write_Byte(uint8_t addr, uint8_t reg, uint8_t data);
void read_camera_data(void);

#ifdef __cplusplus
}
#endif
