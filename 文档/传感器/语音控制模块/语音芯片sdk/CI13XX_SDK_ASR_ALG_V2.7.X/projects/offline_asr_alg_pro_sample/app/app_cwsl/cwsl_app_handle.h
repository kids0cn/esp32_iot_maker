#ifndef __CWSL_APP_HANDLE_H__
#define __CWSL_APP_HANDLE_H__

#include "system_msg_deal.h"
#include "cwsl_manage.h"


typedef enum
{
    ///tag-insert-cwsl-remind-play-id-functioncode-pos-8

    CWSL_DEFAULT = 1000000
}cicwsl_func_index;


////cwsl process ASR message///////////////////////////////////////////////
/**
 * @brief 命令词自学习消息处理函数
 * 
 * @param asr_msg ASR识别结果消息
 * @param cmd_handle 命令词handle
 * @param cmd_id 命令词ID
 * @retval 1 数据有效,消息已处理
 * @retval 0 数据无效,消息未处理
 */
uint32_t cwsl_app_process_asr_msg(sys_msg_asr_data_t *asr_msg, cmd_handle_t *cmd_handle, uint16_t cmd_id);

// cwsl_manage模块复位，用于系统退出唤醒状态时调用
int cwsl_app_reset();

#endif  // __CWSL_APP_HANDLE_H__
