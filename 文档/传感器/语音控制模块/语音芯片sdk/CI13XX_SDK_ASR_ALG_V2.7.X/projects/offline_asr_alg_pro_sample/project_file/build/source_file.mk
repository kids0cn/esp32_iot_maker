# This file is maked by run generate_makefile.lua
C_FLAGS += -DASR_CODE_VERSION=2
OBJS += build/objs/ci130x_init.o
-include build/objs/ci130x_init.d
build/objs/ci130x_init.o : $(SDK_PATH)/startup/ci130x_init.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_vtable.o
-include build/objs/ci130x_vtable.d
build/objs/ci130x_vtable.o : $(SDK_PATH)/startup/ci130x_vtable.S
	$(CC_PREFIX)$(AS) $(S_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_startup.o
-include build/objs/ci130x_startup.d
build/objs/ci130x_startup.o : $(SDK_PATH)/startup/ci130x_startup.S
	$(CC_PREFIX)$(AS) $(S_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_it.o
-include build/objs/ci130x_it.d
build/objs/ci130x_it.o : $(SDK_PATH)/system/ci130x_it.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_system.o
-include build/objs/ci130x_system.d
build/objs/ci130x_system.o : $(SDK_PATH)/system/ci130x_system.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/platform_config.o
-include build/objs/platform_config.d
build/objs/platform_config.o : $(SDK_PATH)/system/platform_config.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_handlers.o
-include build/objs/ci130x_handlers.d
build/objs/ci130x_handlers.o : $(SDK_PATH)/system/ci130x_handlers.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/baudrate_calibrate.o
-include build/objs/baudrate_calibrate.d
build/objs/baudrate_calibrate.o : $(SDK_PATH)/system/baudrate_calibrate.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/debug_time_consuming.o
-include build/objs/debug_time_consuming.d
build/objs/debug_time_consuming.o : $(SDK_PATH)/components/assist/debug_time_consuming.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/croutine.o
-include build/objs/croutine.d
build/objs/croutine.o : $(SDK_PATH)/components/freertos/croutine.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/event_groups.o
-include build/objs/event_groups.d
build/objs/event_groups.o : $(SDK_PATH)/components/freertos/event_groups.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/list.o
-include build/objs/list.d
build/objs/list.o : $(SDK_PATH)/components/freertos/list.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/queue.o
-include build/objs/queue.d
build/objs/queue.o : $(SDK_PATH)/components/freertos/queue.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/stream_buffer.o
-include build/objs/stream_buffer.d
build/objs/stream_buffer.o : $(SDK_PATH)/components/freertos/stream_buffer.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/tasks.o
-include build/objs/tasks.d
build/objs/tasks.o : $(SDK_PATH)/components/freertos/tasks.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/timers.o
-include build/objs/timers.d
build/objs/timers.o : $(SDK_PATH)/components/freertos/timers.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/heap_4.o
-include build/objs/heap_4.d
build/objs/heap_4.o : $(SDK_PATH)/components/freertos/portable/MemMang/heap_4.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci_log.o
-include build/objs/ci_log.d
build/objs/ci_log.o : $(SDK_PATH)/components/log/ci_log.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/FreeRTOS_CLI.o
-include build/objs/FreeRTOS_CLI.d
build/objs/FreeRTOS_CLI.o : $(SDK_PATH)/components/assist/cli/FreeRTOS_CLI.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/UARTCommandConsole.o
-include build/objs/UARTCommandConsole.d
build/objs/UARTCommandConsole.o : $(SDK_PATH)/components/assist/cli/UARTCommandConsole.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/serial.o
-include build/objs/serial.d
build/objs/serial.o : $(SDK_PATH)/components/assist/cli/port/serial.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/Sample_CLI_commands.o
-include build/objs/Sample_CLI_commands.d
build/objs/Sample_CLI_commands.o : $(SDK_PATH)/components/assist/cli/port/Sample_CLI_commands.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci_flash_data_info.o
-include build/objs/ci_flash_data_info.d
build/objs/ci_flash_data_info.o : $(SDK_PATH)/components/flash_control/flash_control_src/ci_flash_data_info.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/flash_control_inner_port.o
-include build/objs/flash_control_inner_port.d
build/objs/flash_control_inner_port.o : $(SDK_PATH)/components/flash_control/flash_control_src/flash_control_inner_port.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ota_aes.o
-include build/objs/ota_aes.d
build/objs/ota_aes.o : $(SDK_PATH)/components/ota/ota_aes.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ota_partition_verify.o
-include build/objs/ota_partition_verify.d
build/objs/ota_partition_verify.o : $(SDK_PATH)/components/ota/ota_partition_verify.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/simple_audio_player.o
-include build/objs/simple_audio_player.d
build/objs/simple_audio_player.o : $(SDK_PATH)/components/simple_audio_player/simple_audio_player.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/player_data_fetcher.o
-include build/objs/player_data_fetcher.d
build/objs/player_data_fetcher.o : $(SDK_PATH)/components/simple_audio_player/player_data_fetcher.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/mp3_decoder_api.o
-include build/objs/mp3_decoder_api.d
build/objs/mp3_decoder_api.o : $(SDK_PATH)/components/simple_audio_player/mp3_decoder_api.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/audio_play_api.o
-include build/objs/audio_play_api.d
build/objs/audio_play_api.o : $(SDK_PATH)/components/player/audio_play/audio_play_api.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/audio_play_decoder.o
-include build/objs/audio_play_decoder.d
build/objs/audio_play_decoder.o : $(SDK_PATH)/components/player/audio_play/audio_play_decoder.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/audio_play_process.o
-include build/objs/audio_play_process.d
build/objs/audio_play_process.o : $(SDK_PATH)/components/player/audio_play/audio_play_process.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/audio_play_os_port.o
-include build/objs/audio_play_os_port.d
build/objs/audio_play_os_port.o : $(SDK_PATH)/components/player/audio_play/audio_play_os_port.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/audio_play_device.o
-include build/objs/audio_play_device.d
build/objs/audio_play_device.o : $(SDK_PATH)/components/player/audio_play/audio_play_device.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/get_play_data.o
-include build/objs/get_play_data.d
build/objs/get_play_data.o : $(SDK_PATH)/components/player/audio_play/get_play_data.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/adpcmdec.o
-include build/objs/adpcmdec.d
build/objs/adpcmdec.o : $(SDK_PATH)/components/player/adpcm/adpcmdec.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/adpcm.o
-include build/objs/adpcm.d
build/objs/adpcm.o : $(SDK_PATH)/components/player/adpcm/adpcm.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/parse_m4a_atom_containers_port.o
-include build/objs/parse_m4a_atom_containers_port.d
build/objs/parse_m4a_atom_containers_port.o : $(SDK_PATH)/components/player/m4a/parse_m4a_atom_containers_port.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/parse_m4a_atom_containers.o
-include build/objs/parse_m4a_atom_containers.d
build/objs/parse_m4a_atom_containers.o : $(SDK_PATH)/components/player/m4a/parse_m4a_atom_containers.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/bitstreamf.o
-include build/objs/bitstreamf.d
build/objs/bitstreamf.o : $(SDK_PATH)/components/player/flacdec/bitstreamf.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/flacdecoder.o
-include build/objs/flacdecoder.d
build/objs/flacdecoder.o : $(SDK_PATH)/components/player/flacdec/flacdecoder.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/tables.o
-include build/objs/tables.d
build/objs/tables.o : $(SDK_PATH)/components/player/flacdec/tables.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/status_share.o
-include build/objs/status_share.d
build/objs/status_share.o : $(SDK_PATH)/components/status_share/status_share.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci_nvdata_manage.o
-include build/objs/ci_nvdata_manage.d
build/objs/ci_nvdata_manage.o : $(SDK_PATH)/components/ci_nvdm/ci_nvdata_manage.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci_nvdata_port.o
-include build/objs/ci_nvdata_port.d
build/objs/ci_nvdata_port.o : $(SDK_PATH)/components/ci_nvdm/ci_nvdata_port.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/command_file_reader.o
-include build/objs/command_file_reader.d
build/objs/command_file_reader.o : $(SDK_PATH)/components/cmd_info/command_file_reader.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/command_info.o
-include build/objs/command_info.d
build/objs/command_info.o : $(SDK_PATH)/components/cmd_info/command_info.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/prompt_player.o
-include build/objs/prompt_player.d
build/objs/prompt_player.o : $(SDK_PATH)/components/cmd_info/prompt_player.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/audio_in_manage_inner.o
-include build/objs/audio_in_manage_inner.d
build/objs/audio_in_manage_inner.o : $(SDK_PATH)/components/audio_in_manage/audio_in_manage_inner.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/asr_malloc_port.o
-include build/objs/asr_malloc_port.d
build/objs/asr_malloc_port.o : $(SDK_PATH)/components/asr/asr_malloc_port.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/asr_process_callback_decoder.o
-include build/objs/asr_process_callback_decoder.d
build/objs/asr_process_callback_decoder.o : $(SDK_PATH)/components/asr/asr_process_callback_decoder.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/asr_process_callback.o
-include build/objs/asr_process_callback.d
build/objs/asr_process_callback.o : $(SDK_PATH)/components/asr/asr_process_callback.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/codec_manager.o
-include build/objs/codec_manager.d
build/objs/codec_manager.o : $(SDK_PATH)/components/codec_manager/codec_manager.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/codec_manage_inner_port.o
-include build/objs/codec_manage_inner_port.d
build/objs/codec_manage_inner_port.o : $(SDK_PATH)/components/codec_manager/codec_manage_inner_port.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/chipintelli_cloud_protocol.o
-include build/objs/chipintelli_cloud_protocol.d
build/objs/chipintelli_cloud_protocol.o : $(SDK_PATH)/components/protocol/chipintelli_cloud_protocol.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/vp_nn_cmpt_port_host.o
-include build/objs/vp_nn_cmpt_port_host.d
build/objs/vp_nn_cmpt_port_host.o : $(SDK_PATH)/components/alg/vp_host/vp_nn_cmpt_port_host.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/voice_print_recognition.o
-include build/objs/voice_print_recognition.d
build/objs/voice_print_recognition.o : $(SDK_PATH)/components/VPR/voice_print_recognition.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/voice_print_wman_recognition.o
-include build/objs/voice_print_wman_recognition.d
build/objs/voice_print_wman_recognition.o : $(SDK_PATH)/components/VPR/voice_print_wman_recognition.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/voice_module_uart_protocol_g1.o
-include build/objs/voice_module_uart_protocol_g1.d
build/objs/voice_module_uart_protocol_g1.o : $(SDK_PATH)/components/msg_com/voice_module_uart_protocol_g1.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/i2c_protocol_module.o
-include build/objs/i2c_protocol_module.d
build/objs/i2c_protocol_module.o : $(SDK_PATH)/components/msg_com/i2c_protocol_module.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/voice_module_uart_protocol.o
-include build/objs/voice_module_uart_protocol.d
build/objs/voice_module_uart_protocol.o : $(SDK_PATH)/components/msg_com/voice_module_uart_protocol.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/voice_module_uart_tts_protocol.o
-include build/objs/voice_module_uart_tts_protocol.d
build/objs/voice_module_uart_tts_protocol.o : $(SDK_PATH)/components/msg_com/voice_module_uart_tts_protocol.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/color_light_control.o
-include build/objs/color_light_control.d
build/objs/color_light_control.o : $(SDK_PATH)/components/led/color_light_control.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/led_light_control.o
-include build/objs/led_light_control.d
build/objs/led_light_control.o : $(SDK_PATH)/components/led/led_light_control.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_adpcm.o
-include build/objs/cias_adpcm.d
build/objs/cias_adpcm.o : $(SDK_PATH)/components/ci_adpcm/cias_adpcm.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_record_demo.o
-include build/objs/cias_record_demo.d
build/objs/cias_record_demo.o : $(SDK_PATH)/components/ci_adpcm/cias_record_demo.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_audio_pre_rslt_out.o
-include build/objs/ci130x_audio_pre_rslt_out.d
build/objs/ci130x_audio_pre_rslt_out.o : $(SDK_PATH)/components/audio_pre_rslt_iis_out/ci130x_audio_pre_rslt_out.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_core_eclic.o
-include build/objs/ci130x_core_eclic.d
build/objs/ci130x_core_eclic.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_core_eclic.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_core_timer.o
-include build/objs/ci130x_core_timer.d
build/objs/ci130x_core_timer.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_core_timer.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_dma.o
-include build/objs/ci130x_dma.d
build/objs/ci130x_dma.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_dma.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_codec.o
-include build/objs/ci130x_codec.d
build/objs/ci130x_codec.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_codec.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_pdm.o
-include build/objs/ci130x_pdm.d
build/objs/ci130x_pdm.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_pdm.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_alc.o
-include build/objs/ci130x_alc.d
build/objs/ci130x_alc.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_alc.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_iisdma.o
-include build/objs/ci130x_iisdma.d
build/objs/ci130x_iisdma.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_iisdma.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_core_misc.o
-include build/objs/ci130x_core_misc.d
build/objs/ci130x_core_misc.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_core_misc.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_iis.o
-include build/objs/ci130x_iis.d
build/objs/ci130x_iis.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_iis.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_adc.o
-include build/objs/ci130x_adc.d
build/objs/ci130x_adc.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_adc.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_gpio.o
-include build/objs/ci130x_gpio.d
build/objs/ci130x_gpio.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_gpio.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_iic.o
-include build/objs/ci130x_iic.d
build/objs/ci130x_iic.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_iic.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_pwm.o
-include build/objs/ci130x_pwm.d
build/objs/ci130x_pwm.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_pwm.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_timer.o
-include build/objs/ci130x_timer.d
build/objs/ci130x_timer.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_timer.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_uart.o
-include build/objs/ci130x_uart.d
build/objs/ci130x_uart.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_uart.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_lowpower.o
-include build/objs/ci130x_lowpower.d
build/objs/ci130x_lowpower.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_lowpower.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_iwdg.o
-include build/objs/ci130x_iwdg.d
build/objs/ci130x_iwdg.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_iwdg.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_spiflash.o
-include build/objs/ci130x_spiflash.d
build/objs/ci130x_spiflash.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_spiflash.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci130x_dtrflash.o
-include build/objs/ci130x_dtrflash.d
build/objs/ci130x_dtrflash.o : $(SDK_PATH)/driver/ci130x_chip_driver/src/ci130x_dtrflash.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/es7243e.o
-include build/objs/es7243e.d
build/objs/es7243e.o : $(SDK_PATH)/driver/third_device_driver/outside_codec/es7243e.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/board.o
-include build/objs/board.d
build/objs/board.o : $(SDK_PATH)/driver/boards/board.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/board_default.o
-include build/objs/board_default.d
build/objs/board_default.o : $(SDK_PATH)/driver/boards/board_default.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/dichotomy_find.o
-include build/objs/dichotomy_find.d
build/objs/dichotomy_find.o : $(SDK_PATH)/utils/dichotomy_find.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/crc.o
-include build/objs/crc.d
build/objs/crc.o : $(SDK_PATH)/utils/crc.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/main.o
-include build/objs/main.d
build/objs/main.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main/main.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/code_switch.o
-include build/objs/code_switch.d
build/objs/code_switch.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main/code_switch.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/system_hook.o
-include build/objs/system_hook.d
build/objs/system_hook.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main/system_hook.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/nlp_msg_deal.o
-include build/objs/nlp_msg_deal.d
build/objs/nlp_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main/nlp_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/system_msg_deal.o
-include build/objs/system_msg_deal.d
build/objs/system_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main/system_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci_ssp_config.o
-include build/objs/ci_ssp_config.d
build/objs/ci_ssp_config.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main/ci_ssp_config.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/user_msg_deal.o
-include build/objs/user_msg_deal.d
build/objs/user_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main/user_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/user_control.o
-include build/objs/user_control.d
build/objs/user_control.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main/user_control.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/doa_app_handle.o
-include build/objs/doa_app_handle.d
build/objs/doa_app_handle.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_doa/doa_app_handle.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cwsl_app_handle.o
-include build/objs/cwsl_app_handle.d
build/objs/cwsl_app_handle.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_cwsl/cwsl_app_handle.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/sed_app_host.o
-include build/objs/sed_app_host.d
build/objs/sed_app_host.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_sed/sed_app_host.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci_nlp_user.o
-include build/objs/ci_nlp_user.d
build/objs/ci_nlp_user.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main/ci_nlp_user.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

LIB_FILES += $(SDK_PATH)/$(LIBS_PATH)/libnlp.a
LD_FLAGS += -L$(SDK_PATH)/$(LIBS_PATH)
LIBS += -lnlp
LIB_FILES += $(SDK_PATH)/$(LIBS_PATH)/libnewlib_port.a
LD_FLAGS += -L$(SDK_PATH)/$(LIBS_PATH)
LIBS += -lnewlib_port
LIB_FILES += $(SDK_PATH)/$(LIBS_PATH)/libfreertos_port.a
LD_FLAGS += -L$(SDK_PATH)/$(LIBS_PATH)
LIBS += -lfreertos_port
LIB_FILES += $(SDK_PATH)/$(LIBS_PATH)/libdsu.a
LD_FLAGS += -L$(SDK_PATH)/$(LIBS_PATH)
LIBS += -ldsu
LIB_FILES += $(SDK_PATH)/$(LIBS_PATH)/libflash_encrypt.a
LD_FLAGS += -L$(SDK_PATH)/$(LIBS_PATH)
LIBS += -lflash_encrypt
LIB_FILES += $(SDK_PATH)/$(LIBS_PATH)/libcwsl_v2.a
LD_FLAGS += -L$(SDK_PATH)/$(LIBS_PATH)
LIBS += -lcwsl_v2
LIB_FILES += $(SDK_PATH)/$(LIBS_PATH)/libtts.a
LD_FLAGS += -L$(SDK_PATH)/$(LIBS_PATH)
LIBS += -ltts
LIB_FILES += $(SDK_PATH)/components/ci_ble/stack/libOnMicroBLE.a
LD_FLAGS += -L$(SDK_PATH)/components/ci_ble/stack
LIBS += -lOnMicroBLE
LIB_FILES += $(SDK_PATH)/components/ci_ble/stack/libcias_crypto.a
LD_FLAGS += -L$(SDK_PATH)/components/ci_ble/stack
LIBS += -lcias_crypto
OBJS += build/objs/app_ble.o
-include build/objs/app_ble.d
build/objs/app_ble.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/app_ble.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/rf_msg_deal.o
-include build/objs/rf_msg_deal.d
build/objs/rf_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/rf_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_rgb_driver.o
-include build/objs/cias_rgb_driver.d
build/objs/cias_rgb_driver.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/demo/cias_rgb_driver.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_rgb_msg_deal.o
-include build/objs/cias_rgb_msg_deal.d
build/objs/cias_rgb_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/demo/cias_rgb_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_fan_msg_deal.o
-include build/objs/cias_fan_msg_deal.d
build/objs/cias_fan_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/demo/cias_fan_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_aircondition_msg_deal.o
-include build/objs/cias_aircondition_msg_deal.d
build/objs/cias_aircondition_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/demo/cias_aircondition_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_warmer_msg_deal.o
-include build/objs/cias_warmer_msg_deal.d
build/objs/cias_warmer_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/demo/cias_warmer_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_heattable_msg_deal.o
-include build/objs/cias_heattable_msg_deal.d
build/objs/cias_heattable_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/demo/cias_heattable_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_tbm_msg_deal.o
-include build/objs/cias_tbm_msg_deal.d
build/objs/cias_tbm_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/demo/cias_tbm_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/cias_waterheated_msg_deal.o
-include build/objs/cias_waterheated_msg_deal.d
build/objs/cias_waterheated_msg_deal.o : $(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/demo/cias_waterheated_msg_deal.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

C_FLAGS += -I$(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble
C_FLAGS += -I$(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_ble/demo
C_FLAGS += -I$(SDK_PATH)/driver/ci130x_chip_driver/inc
C_FLAGS += -I$(SDK_PATH)/driver/boards
C_FLAGS += -I$(SDK_PATH)/driver/third_device_driver/outside_codec
C_FLAGS += -I$(SDK_PATH)/system
C_FLAGS += -I$(SDK_PATH)/components/log
C_FLAGS += -I$(SDK_PATH)/components/assist
C_FLAGS += -I$(SDK_PATH)/components/assist/cli
C_FLAGS += -I$(SDK_PATH)/components/assist/cli/port
C_FLAGS += -I$(SDK_PATH)/components/freertos/include
C_FLAGS += -I$(SDK_PATH)/components/freertos/portable/GCC/N307
C_FLAGS += -I$(SDK_PATH)/components
C_FLAGS += -I$(SDK_PATH)/components/alg/vp_host
C_FLAGS += -I$(SDK_PATH)/components/VPR
C_FLAGS += -I$(SDK_PATH)/components/alg/ai_denoise
C_FLAGS += -I$(SDK_PATH)/components/asr
C_FLAGS += -I$(SDK_PATH)/components/asr/asr_top
C_FLAGS += -I$(SDK_PATH)/components/asr/asr_top/asr_top_inc
C_FLAGS += -I$(SDK_PATH)/components/asr/decoder_v2/decoder_inc
C_FLAGS += -I$(SDK_PATH)/components/asr/vad_fe
C_FLAGS += -I$(SDK_PATH)/components/asr/vad_fe/vad_fe_inc
C_FLAGS += -I$(SDK_PATH)/components/asr/dnn
C_FLAGS += -I$(SDK_PATH)/components/asr/dnn/dnn_inc
C_FLAGS += -I$(SDK_PATH)/components/asr/cinn_v2/cinn_inc
C_FLAGS += -I$(SDK_PATH)/components/asr/npu/npu_inc
C_FLAGS += -I$(SDK_PATH)/components/asr/nn_and_flash
C_FLAGS += -I$(SDK_PATH)/components/asr/nn_and_flash/nn_and_flash_inc
C_FLAGS += -I$(SDK_PATH)/components/fft
C_FLAGS += -I$(SDK_PATH)/components/nlp
C_FLAGS += -I$(SDK_PATH)/components/protocol
C_FLAGS += -I$(SDK_PATH)/components/ci_adpcm
C_FLAGS += -I$(SDK_PATH)/components/msg_com
C_FLAGS += -I$(SDK_PATH)/components/led
C_FLAGS += -I$(SDK_PATH)/components/simple_audio_player
C_FLAGS += -I$(SDK_PATH)/components/player/audio_play
C_FLAGS += -I$(SDK_PATH)/components/player/mp3lib/mp3pub
C_FLAGS += -I$(SDK_PATH)/components/player/aaclib/aacpub
C_FLAGS += -I$(SDK_PATH)/components/player/flacdec
C_FLAGS += -I$(SDK_PATH)/components/player/m4a
C_FLAGS += -I$(SDK_PATH)/components/player/adpcm
C_FLAGS += -I$(SDK_PATH)/components/status_share
C_FLAGS += -I$(SDK_PATH)/components/flash_control/flash_control_inc
C_FLAGS += -I$(SDK_PATH)/components/flash_encrypt
C_FLAGS += -I$(SDK_PATH)/components/codec_manager
C_FLAGS += -I$(SDK_PATH)/components/ci_nvdm
C_FLAGS += -I$(SDK_PATH)/components/cmd_info
C_FLAGS += -I$(SDK_PATH)/components/sys_monitor
C_FLAGS += -I$(SDK_PATH)/components/ota
C_FLAGS += -I$(SDK_PATH)/components/audio_pre_rslt_iis_out
C_FLAGS += -I$(SDK_PATH)/components/audio_in_manage
C_FLAGS += -I$(SDK_PATH)/components/assist/SEGGER
C_FLAGS += -I$(SDK_PATH)/components/assist/SEGGER/config
C_FLAGS += -I$(SDK_PATH)/components/nuclear_com
C_FLAGS += -I$(SDK_PATH)/components/audio_pre_rslt_iis_out
C_FLAGS += -I$(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_cwsl
C_FLAGS += -I$(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_doa
C_FLAGS += -I$(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_main
C_FLAGS += -I$(SDK_PATH)/projects/offline_asr_alg_pro_sample/app/app_sed
C_FLAGS += -I$(SDK_PATH)/utils
C_FLAGS += -I$(SDK_PATH)/components/alg
C_FLAGS += -I$(SDK_PATH)/components/alg/denoise
C_FLAGS += -I$(SDK_PATH)/components/alg/beamforming
C_FLAGS += -I$(SDK_PATH)/components/alg/dereverb
C_FLAGS += -I$(SDK_PATH)/components/alg/alc_auto_switch
C_FLAGS += -I$(SDK_PATH)/components/alg/basic_alg
C_FLAGS += -I$(SDK_PATH)/components/alg/aec
C_FLAGS += -I$(SDK_PATH)/components/alg/doa
C_FLAGS += -I$(SDK_PATH)/components/alg/sound_event_detection
C_FLAGS += -I$(SDK_PATH)/components/aes/
C_FLAGS += -I$(SDK_PATH)/components/ota
C_FLAGS += -I$(SDK_PATH)/components/ci_cwsl_v2
C_FLAGS += -I$(SDK_PATH)/components/alg/pwk/
C_FLAGS += -I$(SDK_PATH)/components/ir_remote_driver
OBJS += build/objs/exe_app_at.o
-include build/objs/exe_app_at.d
build/objs/exe_app_at.o : $(SDK_PATH)/components/ci_ble/app/exe_app_at.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/exe_app_main.o
-include build/objs/exe_app_main.d
build/objs/exe_app_main.o : $(SDK_PATH)/components/ci_ble/app/exe_app_main.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/exe_app_evt.o
-include build/objs/exe_app_evt.d
build/objs/exe_app_evt.o : $(SDK_PATH)/components/ci_ble/app/exe_app_evt.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/ci_ble_spi.o
-include build/objs/ci_ble_spi.d
build/objs/ci_ble_spi.o : $(SDK_PATH)/components/ci_ble/hal/ci_ble_spi.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/exe_adv_init.o
-include build/objs/exe_adv_init.d
build/objs/exe_adv_init.o : $(SDK_PATH)/components/ci_ble/hal/exe_adv_init.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/exe_hal_ci231x.o
-include build/objs/exe_hal_ci231x.d
build/objs/exe_hal_ci231x.o : $(SDK_PATH)/components/ci_ble/hal/exe_hal_ci231x.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/exe_mcu_ci231x.o
-include build/objs/exe_mcu_ci231x.d
build/objs/exe_mcu_ci231x.o : $(SDK_PATH)/components/ci_ble/hal/exe_mcu_ci231x.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/exe_profile_hid.o
-include build/objs/exe_profile_hid.d
build/objs/exe_profile_hid.o : $(SDK_PATH)/components/ci_ble/common/exe_profile_hid.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

OBJS += build/objs/exe_stk_wrapper.o
-include build/objs/exe_stk_wrapper.d
build/objs/exe_stk_wrapper.o : $(SDK_PATH)/components/ci_ble/common/exe_stk_wrapper.c
	$(CC_PREFIX)$(CC) $(C_FLAGS) -c -o "$@" "$<"

C_FLAGS += -I$(SDK_PATH)/components/ci_ble/Inc/ble
C_FLAGS += -I$(SDK_PATH)/components/ci_ble/Inc/hal
C_FLAGS += -I$(SDK_PATH)/components/ci_ble/Inc
C_FLAGS += -I$(SDK_PATH)/components/ci_ble/hal
C_FLAGS += -I$(SDK_PATH)/components/ci_ble/stack
C_FLAGS += -I$(SDK_PATH)/components/tts
C_FLAGS += -I$(SDK_PATH)/components/tts/front_end3
C_FLAGS += -I$(SDK_PATH)/components/tts/vocoder
C_FLAGS += -I$(SDK_PATH)/components/tts/ringbuffer
