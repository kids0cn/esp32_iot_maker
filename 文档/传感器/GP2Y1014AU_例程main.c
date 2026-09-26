#include "stm32f10x.h"
#include "delay.h"
#include "usart.h"
#include "adc.h"
#include "oled.h"
#include "stdio.h"

// === 参数校准 ===
// 遮住传感器时的电压(mV)，通常在500-900之间。如果读出负数，调大此值。
#define NO_DUST_VOLTAGE     400     
// 灵敏度转换系数，通常在 0.17 左右
#define COV_RATIO           0.20f    

// === 全局变量 ===
u16 adc_raw = 0;          // 原始ADC值 (0-4095)
u16 voltage_mv = 0;       // 换算后的电压 (mV)
float dust_density = 0;   // PM2.5浓度 (ug/m3)
char display_buf[32];     // 字符串缓存

// 初始化传感器控制引脚 PA2
void Sensor_LED_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_2; 
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP; // 推挽输出
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOA, &GPIO_InitStructure);
    
    GPIO_ResetBits(GPIOA, GPIO_Pin_2); // 默认低电平(灭)
}

// 均值滤波算法，稳定数值
// === 修复后的滤波算法 ===
u16 Filter_Adc(u16 new_val)
{
    // 定义静态变量，确保数据断电前一直保留
    static u16 buf[10];      // 存放最近10次的数据
    static u8 i = 0;         // 数组下标
    static u32 sum = 0;      // 总和
    static u8 is_first = 1;  // 第一次运行标记

    //如果是第一次进入，把10个位置全部填满当前值
    //这样可以避免刚启动时数据从0缓慢爬升
    if(is_first)
    {
        u8 k;
        for(k=0; k<10; k++) buf[k] = new_val;
        sum = (u32)new_val * 10;
        is_first = 0;
        return new_val;
    }

    // 滑动平均核心逻辑
    sum -= buf[i];          // 1. 减去最老的数据
    buf[i] = new_val;       // 2. 存入最新的数据
    sum += buf[i];          // 3. 加上最新的数据
    
    i++;
    if(i >= 10) i = 0;      // 下标循环

    return (u16)(sum / 10); // 返回平均值
}

// 读取传感器核心逻辑
void Read_GP2Y10(void)
{
    u16 filter_out;
    
    // 1. 开启内部LED (高电平有效)
    GPIO_SetBits(GPIOA, GPIO_Pin_2);
    // 2. 采样前的等待 (严格遵循datasheet: 0.28ms)
    delay_us(280);
    
    // 3. 立即采集电压
    adc_raw = Get_Adc(ADC_Channel_0);
    
    // 4. 补足脉宽并关闭LED (总脉宽0.32ms)
    delay_us(40);
	
    GPIO_ResetBits(GPIOA, GPIO_Pin_2);
    // 5. 传感器恢复时间
    delay_us(9680);
    
    // 6. 滤波处理
    filter_out = Filter_Adc(adc_raw);
    
    // 7. 电压换算 (3300mV参考电压, 12位ADC)
		//*********************************************************//
		//*****转接板是1K跟10K的电阻分压采集，所以下面要乘11*******//
		//*********************************************************//
		voltage_mv = (u16)( ( (unsigned long)filter_out * 3300 ) / 4096 )* 11;    
    // 8. 浓度计算
    if(voltage_mv > NO_DUST_VOLTAGE)
    {
        dust_density = (float)(voltage_mv - NO_DUST_VOLTAGE) * COV_RATIO;
    }
    else
    {
        dust_density = 0;
    }
}

int main(void)
{	 
    // 系统初始化
    delay_init();	    	 
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    uart_init(9600);	 	 
    
    // 模块初始化
    Adc_Init();           // PA0 初始化
    Sensor_LED_Init();    // PA2 初始化
    OLED_Init();          // OLED 初始化
    OLED_Clear(); 
    
    // 静态界面显示
    OLED_ShowString(0, 0, "GP2Y10 Monitor", 16);
    OLED_ShowString(0, 2, "Raw:      ", 16);
    OLED_ShowString(0, 4, "Vol:      mV", 16);
    OLED_ShowString(0, 6, "PM :      ug", 16);
    
    printf("System Init OK.\r\n");

    while(1)
    {
        // 读取数据
        Read_GP2Y10();
        
        // --- OLED 显示刷新 ---
        
        // 显示原始值
        sprintf(display_buf, "%4d", adc_raw);
        OLED_ShowString(40, 2, display_buf, 16);
        
        // 显示电压
        sprintf(display_buf, "%4d", voltage_mv);
        OLED_ShowString(40, 4, display_buf, 16);
        
        // 显示浓度 (保留1位小数)
        sprintf(display_buf, "%4.1f", dust_density);
        OLED_ShowString(40, 6, display_buf, 16);
        
        // --- 串口打印 ---
        printf("Raw:%d, Vol:%d mV, PM:%.1f\r\n", adc_raw, voltage_mv, dust_density);
        
        // 延时
        delay_ms(200); 
    }
}

