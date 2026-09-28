#include "FreeRTOS.h"
#include "task.h"
#include "timers.h"
#include "system_msg_deal.h"
#include "prompt_player.h"
#include "voice_module_uart_protocol.h"
#include "i2c_protocol_module.h"
#include "ci_nvdata_manage.h"
#include "ci_log.h"
#include "ci130x_gpio.h"
#include "voice_print_recognition.h"
#include "user_msg_deal.h"
#include "user_control.h"
// #include "all_cmd_statement.h"

static send_cmd_pro_t welcome_pro[] = {
    ///tag-insert-send-welcome-protocol-pos-9
    //send_welcome_protocol_function
    {10001,"\xA5\xFA\x00\x81\x0A\x00\x2A\xFB",8,1},  //欢迎语

};

static send_cmd_pro_t bye_pro[] = {
    ///tag-insert-send-bye-protocol-pos-10
    //send_bye_protocol_function
    {10002,"\xA5\xFA\x00\x81\x0B\x00\x2B\xFB",8,1},  //休息语

};

static recv_cmd_pro_t recv_data[] = {
    ///tag-deal-uart-msg-by-play-id-after-recv-start
    //play_msg_deal_by_cmd_id
    {1,"\xA5\xFA\x00\x82\x01\x00\x21\xFB",8,0},  //你好小丹
    {2,"\xA5\xFA\x00\x82\x02\x00\x22\xFB",8,0},  //开灯
    {3,"\xA5\xFA\x00\x82\x02\x00\x23\xFB",8,0},  //打开风扇
    {4,"\xA5\xFA\x00\x82\x02\x00\x24\xFB",8,0},  //关灯
    {5,"\xA5\xFA\x00\x82\x03\x00\x25\xFB",8,0},  //关闭风扇
    {10,"\xA5\xFA\x00\x82\x03\x00\x26\xFB",8,0},  //开空调
    {11,"\xA5\xFA\x00\x82\x03\x00\x27\xFB",8,0},  //关空调
    {12,"\xA5\xFA\x00\x82\x03\x00\x28\xFB",8,0},  //开抽湿机
    {13,"\xA5\xFA\x00\x82\x02\x00\x25\xFB",8,0},  //关抽湿机
    {10001,"\xA5\xFA\x00\x82\x0A\x00\x2B\xFB",8,0},  //欢迎语
    {10002,"\xA5\xFA\x00\x82\x0B\x00\x2C\xFB",8,0},  //休息语

};

static send_cmd_pro_t send_data[] = {
    ///tag-send-uart-msg-by-cmd-id-after-has-asr-result-start
    //asr_msg_deal_by_cmd_id
    {1,"\xA5\xFA\x00\x81\x01\x00\x21\xFB",8,1},  //你好小丹
    {2,"\xA5\xFA\x00\x81\x02\x00\x22\xFB",8,1},  //开灯
    {3,"\xA5\xFA\x00\x81\x02\x00\x23\xFB",8,1},  //打开风扇
    {4,"\xA5\xFA\x00\x81\x02\x00\x24\xFB",8,1},  //关灯
    {5,"\xA5\xFA\x00\x81\x03\x00\x25\xFB",8,1},  //关闭风扇
    {10,"\xA5\xFA\x00\x81\x03\x00\x26\xFB",8,1},  //开空调
    {11,"\xA5\xFA\x00\x81\x03\x00\x27\xFB",8,1},  //关空调
    {12,"\xA5\xFA\x00\x81\x03\x00\x28\xFB",8,1},  //开抽湿机
    {13,"\xA5\xFA\x00\x81\x02\x00\x25\xFB",8,1},  //关抽湿机

};

// {"cmd_id","wakeup_net"},
wakeup_data wakeup_lst[] = {
///tag-insert-multi-wake-up-net-switch-wakeup-lst-pos-13
//multi_wake_up_net_switch_wakeup_lst_function
    {1,1}, //你好小丹


};

static wakeup_net_pro_t wakeup_net_recv_prot[] = {
    ///tag-insert-multi-wake-up-net-switch-by-recv-protocol-pos-14
};

///tag-insert-uart-send-data-function-code-pos-1
//uart_send_data_function
void uart_send_bytes(char *buffer,int length)
{
	for(int index = 0; index < length; index++)
	{
		UartPollingSenddata((UART_TypeDef*)UART_PROTOCOL_NUMBER, buffer[index]);
	}
}


///tag-insert-uart-recv-function-code-pos-2
//uart_recv_data_function
void send_packet_rev_msg(void *msg, int msg_lenght)
{
	sys_msg_t send_msg;
	BaseType_t xResult;
	BaseType_t xHigherPriorityTaskWoken = pdFALSE;
	send_msg.msg_type = SYS_MSG_TYPE_COM;
	if(msg_lenght <= sizeof(send_msg.msg_data.com_data))
	{
		memcpy((uint8_t *)(&send_msg.msg_data.com_data), msg, msg_lenght);
		xResult = send_msg_to_sys_task(&send_msg,&xHigherPriorityTaskWoken);
		if((xResult != pdFAIL)&&(pdTRUE == xHigherPriorityTaskWoken))
		{
			portYIELD_FROM_ISR(xHigherPriorityTaskWoken);
		}
	}
}

#define BUF_LEN (20)

static char rx_buffer[BUF_LEN];
void uart_irq_handler(void)
{
	if (((UART_TypeDef*)UART_PROTOCOL_NUMBER)->UARTMIS & (1UL << UART_RXTimeoutInt))
	{
		rx_buffer[0] = 1; //第一个字节存放数据长度
		while(0 == (((UART_TypeDef*)UART_PROTOCOL_NUMBER)->UARTFlag & (0x1 << UART_RXFE)))
		{
			if (rx_buffer[0] >= BUF_LEN)
			{
				memmove(rx_buffer+1, rx_buffer+2, BUF_LEN - 1);
				rx_buffer[0] = BUF_LEN;
			}
			rx_buffer[rx_buffer[0]++] = UART_RXDATA((UART_TypeDef*)UART_PROTOCOL_NUMBER);
		}
		send_packet_rev_msg(&rx_buffer, rx_buffer[0]);
	}
	UART_IntClear((UART_TypeDef*)UART_PROTOCOL_NUMBER,UART_AllInt);
}



///tag-insert-insert-baudrate-calibrate-send-function-code-pos-3
//baudrate_calibrate_send_function
#if (UART_BAUDRATE_CALIBRATE == 1)
void baudrate_calibrate_set_ack(void);
void defined_send_baudrate_sync_req(void)
{
	uart_send_bytes("\xA5\xFA\x00\x80\x0A\x00\x21\xFB",8);
}
#endif






// 接收到协议后的播报
void deal_voice_by_recv_protocol(char *p_data,char length, int* max_vol_id,int* min_vol_id)
{
    int vol_id = 0;
    int recv_pro_len = sizeof(recv_data)/sizeof(recv_cmd_pro_t);
    for(int i=0;i<recv_pro_len;i++)
    {
        vol_id = recv_data[i].paly_type;
        if(vol_id == PLAY_MAXIMUM_VOLUME)
        {
            *max_vol_id = recv_data[i].cmd_id;
        }
        if(vol_id == PLAY_MINIMUM_VOLUME)
        {
            *min_vol_id = recv_data[i].cmd_id;
        }
        if(memcmp(p_data,recv_data[i].pro_buf,recv_data[i].pro_len) == 0)
        {
            if(vol_id == PLAY_MAXIMUM_VOLUME)
            {
                vol_set(VOLUME_MAX);
            }
            else if(vol_id == PLAY_MINIMUM_VOLUME)
            {
                vol_set(VOLUME_MIN);
            }
            else if(vol_id == PLAY_TURN_UP_VOLUME)
            {
                vol_set(vol_get() + 1);
            }
            else if(vol_id == PLAY_TURN_DOWN_VOLUME)
            {
                vol_set(vol_get() - 1);
            }
        }
    }
}


void change_wakeup_net_by_recv_protocol(char *p_data,char length)
{
    uint8_t wakeup_net;
    uint16_t play_id = 0;
    int i = 0;

    int net_len = sizeof(wakeup_net_recv_prot)/sizeof(wakeup_net_pro_t);
    for(;i<net_len;i++)
    {
        if (memcmp(p_data, wakeup_net_recv_prot[i].pro_buf, wakeup_net_recv_prot[i].pro_len) == 0)
        {
            wakeup_net = wakeup_net_recv_prot[i].net_num;
            play_id = wakeup_net + PLAY_ID_OFFSET_SWITCH;
            set_wakeup_net(wakeup_net);
            #if (UART_PROTOCOL_VER == 255)
                pause_voice_in();
                prompt_play_by_cmd_id(play_id, -1, default_play_done_callback,true);
            #endif
            break;
        }
    }
}


//
extern void set_state_enter_wakeup(uint32_t exit_wakup_ms);      /* 设置退出唤醒时间 */
void com_msg_process(char * msg)
{
    int i = 0;
    int cmd_id = -1;
    int vol_id = -1;
    char length  = msg[0] - 1;
    char *p_data = msg + 1;
    int max_vol_id = -1;
    int min_vol_id = -1;
    
    deal_voice_by_recv_protocol(p_data,length,&max_vol_id,&min_vol_id);
    change_wakeup_net_by_recv_protocol(p_data,length);
    // 接收到协议后的播报
    int recv_pro_len = sizeof(recv_data)/sizeof(recv_cmd_pro_t);
    for(;i<recv_pro_len;i++)
    {
        if(memcmp(p_data,recv_data[i].pro_buf,recv_data[i].pro_len) == 0)
        {
            cmd_id = recv_data[i].cmd_id;
            vol_id = recv_data[i].paly_type;
            // 如果协议是唤醒词的，则需要切换到命令词网络s
            if(check_cmd_id_is_wake_up(cmd_id))
            {
                change_asr_normal_word();
                set_state_enter_wakeup(EXIT_WAKEUP_TIME);
            }
            // 播报
            if((vol_id == PLAY_TURN_UP_VOLUME) && (vol_get() == VOLUME_MAX))
            {
                cmd_id = max_vol_id;
            }
            if((vol_id == PLAY_TURN_DOWN_VOLUME) && (vol_get() == VOLUME_MIN))
            {
                cmd_id = min_vol_id;
            }
        }
    }

    ///tag-insert-baudrate-calibrate-recv-functioncode-pos-4
    //baudrate_calibrate_recv_function
    #if (UART_BAUDRATE_CALIBRATE == 1)
if (memcmp(p_data, "\xA5\xFA\x00\x80\x0A\x00\x22\xFB", length) == 0)
	{
		baudrate_calibrate_set_ack();
	}
#endif



    //播报
    pause_voice_in();
    prompt_play_by_cmd_id(cmd_id, -1, default_play_done_callback,true);
}

uint8_t check_cmd_id_is_wake_up(uint16_t cmd_id)
{
    int i = 0;
    int net_num = 0;
    int wakeup_num = sizeof(wakeup_lst) / sizeof(wakeup_data);
    for(; i < wakeup_num; i++)
    {
        if(wakeup_lst[i].wakeup_cmd_id == cmd_id)
        {
            net_num = 1;
            break;
        }
    }
    return net_num;
}

uint8_t get_wakeup_net_by_cmd_id(uint16_t cmd_id)
{
    int i = 0;
    int net_num = 1;
    int wakeup_num = sizeof(wakeup_lst) / sizeof(wakeup_data);
    for(; i < wakeup_num; i++)
    {
        if(wakeup_lst[i].wakeup_cmd_id == cmd_id)
        {
            net_num = wakeup_lst[i].wakeup_net;
            break;
        }
    }
    return net_num;
}


uint32_t deal_welcome_for_usr(void)
{
    int i = 0;
    for(;i<sizeof(welcome_pro)/sizeof(send_cmd_pro_t);i++)
    {
        uart_send_bytes(welcome_pro[i].pro_buf,welcome_pro[i].pro_len);
    }
}


uint32_t deal_exit_wakeup_for_usr(void)
{
    if(power_on_flag)
    {
        power_on_flag = 0;
    }
    else
    {
        int i = 0;
        for(;i<sizeof(bye_pro)/sizeof(send_cmd_pro_t);i++)
        {
            uart_send_bytes(bye_pro[i].pro_buf,bye_pro[i].pro_len);
        }
    }
}


int check_change_cmd_id(uint16_t cmd_id)
{
    int ret = 0;
    int i = 0;
    for(;i<sizeof(wakeup_net_recv_prot)/sizeof(wakeup_net_pro_t);i++)
    {
        if(cmd_id == wakeup_net_recv_prot[i].cmd_id)
        {
            ret = 1;
            break;
        }
    }
    return ret;
}


void set_wake_up_net_by_asr(uint16_t cmd_id)
{
    uint8_t wakeup_net;
    uint16_t real_len;
    //
    if((cmd_id != SWITCH_WAKEWORD_ID) && (!check_change_cmd_id(cmd_id)))
    {
        return;
    }
    if (CINV_OPER_SUCCESS != cinv_item_read(NVDATA_ID_USER_WAKEUP_NET, sizeof(wakeup_net), &wakeup_net, &real_len))
    {
        wakeup_net = 1;
        cinv_item_init(NVDATA_ID_USER_WAKEUP_NET, sizeof(wakeup_net), &wakeup_net);
    }
    else
    {
        if(cmd_id == SWITCH_WAKEWORD_ID)
        {
            wakeup_net += 1;
            if(wakeup_net > WAKE_UP_NET_TOTAL)
            {
                wakeup_net = 1;
            }
        }
        else
        {
            wakeup_net = get_wakeup_net_by_cmd_id(cmd_id);
        }
    }
    set_wakeup_net(wakeup_net);
    // 
    int play_id = wakeup_net + PLAY_ID_OFFSET_SWITCH;
    pause_voice_in();
    prompt_play_by_cmd_id(play_id, -1, default_play_done_callback,true);
}

/**
 * @brief 用户初始化
 *
 */
void userapp_initial(void)
{
    #if CPU_RATE_PRINT
    init_timer3_getresource();
    #endif

    #if MSG_COM_USE_UART_EN
    #if (UART_PROTOCOL_VER == 1)
    uart_communicate_init();
    #elif (UART_PROTOCOL_VER == 2)
    vmup_communicate_init();
    #elif (UART_PROTOCOL_VER == 255)
    
    #if HAL_UART0_BASE == UART_PROTOCOL_NUMBER
    __eclic_irq_set_vector(UART0_IRQn, (int32_t)uart_irq_handler);
    #elif (HAL_UART1_BASE == UART_PROTOCOL_NUMBER)
    __eclic_irq_set_vector(UART1_IRQn, (int32_t)uart_irq_handler);
    #else
    __eclic_irq_set_vector(UART2_IRQn, (int32_t)uart_irq_handler);
    #endif

    UARTInterruptConfig((UART_TypeDef *)UART_PROTOCOL_NUMBER, UART_PROTOCOL_BAUDRATE);
    UART_IntMaskConfig((UART_TypeDef *)UART_PROTOCOL_NUMBER, UART_RXTimeoutInt, DISABLE);
    UART_IntMaskConfig((UART_TypeDef *)UART_PROTOCOL_NUMBER, UART_RXInt, ENABLE);
    #if UART_BAUDRATE_CALIBRATE
    baudrate_calibrate_init(UART_PROTOCOL_NUMBER, UART_PROTOCOL_BAUDRATE, defined_send_baudrate_sync_req);          // 初始化波特率校准
    baudrate_calibrate_start();         // 启动一次波特率校准
    #endif
    #endif
    #endif

    #if MSG_USE_I2C_EN
    i2c_communicate_init();
    #endif
    #if USE_IR_ENABLE   //红外任务初始化
    ir_task_init(); 
    get_flash_eut_state();   //获取下自动测试状态
    xTaskCreate(DeviceTaskProcess, "DeviceTaskProcess", 300, NULL, 4, NULL);
    #endif

    ///tag-gpio-init
    user_pin_control_init();
}

/**
 * @brief 处理按键消息（目前未实现该demo）
 *
 * @param key_msg 按键消息
 */
void userapp_deal_key_msg(sys_msg_key_data_t  *key_msg)
{
    (void)(key_msg);
}



/**
 * @brief 按语义ID响应asr消息处理
 *
 * @param asr_msg
 * @param cmd_handle
 * @param semantic_id
 * @return uint32_t
 */
uint32_t deal_asr_msg_by_semantic_id(sys_msg_asr_data_t *asr_msg, cmd_handle_t cmd_handle, uint32_t semantic_id)
{
    uint32_t ret = 1;
    if (PRODUCT_GENERAL == get_product_id_from_semantic_id(semantic_id))
    {
        uint8_t vol;
        int select_index = -1;
        switch(get_function_id_from_semantic_id(semantic_id))
        {
        case VOLUME_UP:        //增大音量
            vol = vol_set(vol_get() + 1);
            select_index = (vol == VOLUME_MAX) ? 1:0;
            break;
        case VOLUME_DOWN:      //减小音量
            vol = vol_set(vol_get() - 1);
            select_index = (vol == VOLUME_MIN) ? 1:0;
            break;
        case MAXIMUM_VOLUME:   //最大音量
            vol_set(VOLUME_MAX);
            break;
        case MEDIUM_VOLUME:  //中等音量
            vol_set(VOLUME_MID);
            break;
        case MINIMUM_VOLUME:   //最小音量
            vol_set(VOLUME_MIN);
            break;
        case TURN_ON_VOICE_BROADCAST:    //开启语音播报
            prompt_player_enable(ENABLE);
            break;
        case TURN_OFF_VOICE_BROADCAST:    //关闭语音播报
            prompt_player_enable(DISABLE);
            break;
        default:
            ret = 0;
            break;
        }
        if (ret)
        {
            #if PLAY_OTHER_CMD_EN && !WMAN_PLAY_EN
            #if MULT_INTENT > 1
            prompt_play_by_cmd_handle(cmd_handle, select_index, default_play_done_callback,false);
            #else
            prompt_play_by_cmd_handle(cmd_handle, select_index, default_play_done_callback,true);
            #endif
            #endif
        }
    }
    else
    {
        ret = 0;
    }
    return ret;
}

#if USE_VPR
bool vpr_delete_tmp_flag = 0;
bool vpr_delete_all_flag = 0;
#endif

/**
 * @brief 按命令词id响应asr消息处理
 *
 * @param asr_msg
 * @param cmd_handle
 * @param cmd_id
 * @return uint32_t
 */
uint32_t deal_asr_msg_by_cmd_id(sys_msg_asr_data_t *asr_msg, cmd_handle_t cmd_handle, uint16_t cmd_id)
{
    uint32_t ret = 1;
    int select_index = -1;
    int play_id = 0;
    play_id = cmd_id;
    switch(cmd_id)
    {   
        case -1:
            ret = 0;
        break;
        ///tag-insert-control-volume-code-pos-6

#if USE_VPR
        ///tag-insert-voiceprint-status-pos-19
        //voiceprint_status_function
        case 1: //注册声纹
        {
            extern int vpr_start_regist();
            if(vpr_start_regist() == -1)
            {
                select_index = 1;
            }
            break;
        }
        case 1://删除声纹
        {
            vpr_delete_tmp_flag = 1;
            vpr_delete_all_flag = 0;
            break;
        }
        case 1://删除全部声纹
        {
            vpr_delete_tmp_flag = 0;
            vpr_delete_all_flag = 1;
            break;
        }
        case 1://确定删除
        {
            if (vpr_delete_tmp_flag)
            {
                extern int vpr_delete();
                if(vpr_delete() == -1)
                {
                    select_index = 1;
                }
                vpr_delete_tmp_flag = 0;
            }
            else if (vpr_delete_all_flag)
            {
                extern int vpr_clear();
                vpr_clear();
                vpr_delete_all_flag = 0;
            }
            else
            {
                select_index = 1;
            }
            break;
        }


        
#endif
        ///tag-asr-msg-deal-by-cmd-id-end
        default:
            ret = 0;
            break;
    }
#if CIAS_BLE_CONNECT_MODE_ENABLE
    deal_ble_send_msg(cmd_id);
#endif
    if (ret && select_index >= -1)
    {
        // #if PLAY_OTHER_CMD_EN && !WMAN_PLAY_EN
		#if 1
        #if MULT_INTENT > 1
        prompt_play_by_cmd_id(play_id, select_index, default_play_done_callback,false);
        #else
        prompt_play_by_cmd_id(play_id, select_index, default_play_done_callback,true);
        #endif
        #endif
    }

    return ret;
}

/**
 * @brief 用户自定义消息处理
 *
 * @param msg
 * @return uint32_t
 */
uint32_t deal_userdef_msg(sys_msg_t *msg)
{
    uint32_t ret = 1;
    switch(msg->msg_type)
    {
    /* 按键消息 */
    case SYS_MSG_TYPE_KEY:
    {
        sys_msg_key_data_t *key_rev_data;
        key_rev_data = &msg->msg_data.key_data;
        userapp_deal_key_msg(key_rev_data);
        break;
    }
    #if MSG_COM_USE_UART_EN
    /* CI串口协议消息 */
    case SYS_MSG_TYPE_COM:
    {
		#if ((UART_PROTOCOL_VER == 1) || (UART_PROTOCOL_VER == 2))
    	sys_msg_com_data_t *com_rev_data;
        com_rev_data = &(msg->msg_data.com_data);
        userapp_deal_com_msg(com_rev_data);
        #elif (UART_PROTOCOL_VER == 255)
        com_msg_process(&msg->msg_data.com_data); //串口数据接收时,rx_buffer[0]存放的是接收的数据长度信息，串口数据从rx_buffer[1]开始存放
        #endif
        break;
    }
    #endif
    /* CI IIC 协议消息 */
    #if MSG_USE_I2C_EN
    case SYS_MSG_TYPE_I2C:
    {
        sys_msg_i2c_data_t *i2c_rev_data;
        i2c_rev_data = &msg->msg_data.i2c_data;
        userapp_deal_i2c_msg(i2c_rev_data);
        break;
    }
    #endif
    default:
        break;
    }
    return ret;
}


/**
 * @brief 按命令词id响应asr消息处理
 *
 * @param asr_msg
 * @param cmd_handle
 * @param cmd_id
 * @return uint32_t
 */
uint32_t deal_asr_msg_by_cmd_id_for_usr(cmd_handle_t cmd_handle)
{
    uint32_t ret = 0;
    int select_index = -1;
    uint16_t cmd_id = cmd_info_get_command_id(cmd_handle);
    #if (UART_PROTOCOL_VER == 255)
    if(MULTI_WAKE_UP_NET_SWITCH)
    {
        set_wake_up_net_by_asr(cmd_id);
        if(cmd_info_is_wakeup_word(cmd_handle))
        {
            if(get_wakeup_net_by_cmd_id(cmd_id) != get_wakeup_net())
            {
                return ret;
            }
            else
            {
                cmd_info_change_cur_model_group(0);
            }
        }
    }
    #endif
    // 识别到命令词后的发送协议
    int send_pro_len = sizeof(send_data)/sizeof(send_cmd_pro_t);
    int i=0;
    for(;i<send_pro_len;i++)
    {
        if(cmd_id == send_data[i].cmd_id)
        {
            uart_send_bytes(send_data[i].pro_buf,send_data[i].pro_len);
            ret = send_data[i].paly_type;
        }
    }
    if (ret && select_index >= -1)
    {
        // 声纹注册播报和正常识别播报兼容
        #if USE_VPR
        extern int8_t g_voice_print_rec_index;
        extern int aivpr_status;
        if((g_voice_print_rec_index == -1) && (vpr_get_current_reg_id() != 0) && \
            (vpr_get_status() == 0) && \
            (cmd_info_is_wakeup_word(cmd_handle))) {
            aivpr_status = 0;
            return ret;
        }
        if((g_voice_print_rec_index != -1) && \
            (vpr_get_status() == 0) && \
            (cmd_info_is_wakeup_word(cmd_handle))) {
            aivpr_status = 0;
            return ret;
        }
        if (vpr_get_status() == 1)return ret;
        if (aivpr_status == 1) {
            aivpr_status = 0;
            return ret;
        }
        # endif
        #if (UART_PROTOCOL_VER == 255) && !WMAN_PLAY_EN
        pause_voice_in();
        if(MULT_INTENT > 1 && !cmd_info_is_wakeup_word(cmd_handle)){
            prompt_play_by_cmd_handle(cmd_handle, select_index, default_play_done_callback,false);
        } else {
            prompt_play_by_cmd_handle(cmd_handle, select_index, default_play_done_callback,true);
        }
        #else
        #if PLAY_OTHER_CMD_EN && !WMAN_PLAY_EN
        pause_voice_in();
        if(MULT_INTENT > 1 && !cmd_info_is_wakeup_word(cmd_handle)){
            prompt_play_by_cmd_handle(cmd_handle, select_index, default_play_done_callback,false);
        } else {
            prompt_play_by_cmd_handle(cmd_handle, select_index, default_play_done_callback,true);
        }
        #endif
        #endif
    }

    return ret;
}
