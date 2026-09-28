1. For detailed information about the SDK's components, features, and development, please refer to the SDK documentation for the corresponding chip series in the Software Development section of the Docume
   Taking CI13060 as an example, the reference link is as follows: https://document.chipintelli.com/%E8%BD%AF%E4%BB%B6%E5%BC%80%E5%8F%91/SDK/CI130X%E8%8A%AF%E7%89%87SDK/CI-SDK-Offline/

2. If you need to modify and adjust the relevant algorithm parameters in the SDK, please refer to the SDK documentation for the corresponding chip series in the Software Development section of the Documentation Center.
   Taking AEC as an example, the reference link is as follows:：https://document.chipintelli.com/%E8%BD%AF%E4%BB%B6%E5%BC%80%E5%8F%91/SDK/CI130X%E8%8A%AF%E7%89%87SDK/components/%E5%9B%9E%E5%A3%B0%E6%B6%88%E9%99%A4%E4%BD%BF%E7%94%A8%E8%AF%B4%E6%98%8E/

3. If the firmware size exceeds the flash capacity of the selected chip, please try again after switching to a smaller acoustic model, reducing the number of command words or voice prompts, or using a chip with larger flash capacity.

4. If algorithms are enabled, the system provides relatively less memory space for language model. To ensure normal recognition operation, please reduce the number of command words.


The firmware communication protocol is as follows:
你好小丹:A5 FA 00 81 01 00 21 FB:A5 FA 00 82 01 00 21 FB
开灯:A5 FA 00 81 02 00 22 FB:A5 FA 00 82 02 00 22 FB
打开风扇:A5 FA 00 81 02 00 23 FB:A5 FA 00 82 02 00 23 FB
关灯:A5 FA 00 81 02 00 24 FB:A5 FA 00 82 02 00 24 FB
关闭风扇:A5 FA 00 81 03 00 25 FB:A5 FA 00 82 03 00 25 FB
开风扇:A5 FA 00 81 02 00 23 FB:A5 FA 00 82 02 00 23 FB
关风扇:A5 FA 00 81 03 00 25 FB:A5 FA 00 82 03 00 25 FB
开空调:A5 FA 00 81 03 00 26 FB:A5 FA 00 82 03 00 26 FB
关空调:A5 FA 00 81 03 00 27 FB:A5 FA 00 82 03 00 27 FB
开抽湿机:A5 FA 00 81 03 00 28 FB:A5 FA 00 82 03 00 28 FB
关抽湿机:A5 FA 00 81 02 00 25 FB:A5 FA 00 82 02 00 25 FB
<欢迎语>:A5 FA 00 81 0A 00 2A FB:A5 FA 00 82 0A 00 2B FB
<休息语>:A5 FA 00 81 0B 00 2B FB:A5 FA 00 82 0B 00 2C FB
