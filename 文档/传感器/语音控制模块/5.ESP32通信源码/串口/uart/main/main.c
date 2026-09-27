#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "uart0.h"
#include "uart1.h"

void Write_Data(uint8_t dat);

uint8_t  cmd[5];
uint8_t i2c_rxbuffer[25];
uint8_t bufnum = 0;
void app_main(void)
{

    Uart0_Init();
    Uart1_Init();

    uint16_t uart0_rx_len = 0;
    uint16_t uart1_rx_len = 0;
    int i = 0;
    Write_Data(init);

    while (1)
    {
        uart0_rx_len = Uart0_Available();
        if (uart0_rx_len)
        {
            for (i = 0; i < uart0_rx_len; i++)
            {
                Uart1_Send_Byte(Uart0_Read());
            }
        }

        uart1_rx_len = Uart1_Available();
        if (uart1_rx_len)
        {
            for (i = 0; i < uart1_rx_len; i++)
            {
                i2c_rxbuffer[bufnum++] = Uart1_Read();
                if (bufnum >=5)
                {
                    bufnum = 0;
                    Uart0_Send_Byte(i2c_rxbuffer[3]);
                }   
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void Write_Data(uint8_t dat)
{

	cmd[0] = 0xAA;
	cmd[1] = 0x55;
	cmd[2] = 0xFF;
	cmd[3] = dat;  // 直接使用传入的数据
	cmd[4] = 0xFB;

	for(int j = 0; j<5 ;j++)
	{
		Uart1_Send_Byte(cmd[j]);
	}
   

}







