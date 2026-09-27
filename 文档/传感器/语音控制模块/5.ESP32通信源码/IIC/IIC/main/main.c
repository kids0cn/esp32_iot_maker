#include <stdio.h>

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "i2c_master.h"



void app_main(void)
{
    printf("hello yahboom\n");

    I2C_Master_Init();

    I2C_Master_Write_Byte(VoicceADDR,Write_register,init);

    while (1)
    {
        read_camera_data();
    }
    
}
