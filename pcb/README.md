# WM8978 USB 声卡硬件说明

主原理图为 `WM8978.kicad_sch`，功能已按信号流拆成 4 个子页：

- `01_usb_power.kicad_sch`：USB Type-C、CC 下拉、USB ESD/串联电阻和 3.3 V 电源
- `02_stm32.kicad_sch`：STM32F407、8 MHz HSE、复位、BOOT0、SWD 和 MCU 去耦
- `03_wm8978.kicad_sch`：WM8978、两线控制接口、I2S、VMID 和 codec 去耦
- `04_audio_output.kicad_sch`：3.5 mm 耳机输出和 BTL 扬声器接口

## 当前功能链路

- USB-C A6/B6（D+）经 D1、R53 接 STM32 PA12。
- USB-C A7/B7（D-）经 D2、R54 接 STM32 PA11。
- CC1、CC2 分别用 5.1 kΩ 下拉，接口工作在 USB 设备/UFP 模式。
- STM32 I2S2 使用 PC6/MCLK、PB13/BCLK、PB12/LRCLK、PC3/DACDAT。
- WM8978 控制接口使用 PD6/SCLK、PD7/SDIN，CSB 接地，MODE 用 10 kΩ 下拉，
  因而采用两线控制模式。
- WM8978 ROUT1/LOUT1 经 220 µF 隔直后接 3.5 mm 耳机座。
- ROUT2/LOUT2 作为 BTL 差分扬声器输出；SPK_P、SPK_N 都不能接地。

## 与旧版单页原理图相比

保留了 USB 播放、耳机输出、WM8978 内置扬声器驱动、SWD 和复位功能；删除了
麦克风/模拟采集、LSE、外置 24.576 MHz 音频振荡器、EEPROM、状态 LED、启动
选择开关和 TDA2822 外置功放。新版本由 STM32 作为 I2S 时钟主机，旧版固件中
“WM8978 + 24.576 MHz 振荡器作为时钟主机”的初始化逻辑不能直接沿用。

## 供电与器件约束

- USB VBUS 直接给 WM8978 SPKVDD，3.3 V 给 STM32 和 WM8978 的 AVDD/DCVDD/DBVDD。
- 3.3 V 稳压器指定为与 AMS1117 引脚兼容的 `TLV1117LV33DCYR`，可稳定驱动现有
  陶瓷输入/输出电容；不要在不核对输出电容 ESR 要求的情况下换回泛化 AMS1117。
- C83（VMID）使用 4.7 µF/0805；C77、C78、C89、C93 使用 10 µF/0805。
- C88、C90、C91 分别作为 WM8978 三个 3.3 V 电源脚的 100 nF 就地去耦，C89
  提供 codec 本地 10 µF 储能；C92/C93 给 SPKVDD 去耦。
- C86、C87 为 220 µF/6.3 V 贴片电解，正极朝向 WM8978 输出端。
- X3 为 8 MHz、CL=12 pF 晶体，C68/C69 为 18 pF；正式选定料号后仍需按实际
  杂散电容复核负载电容。

## PCB 实现要点

- D1/D2 紧贴 USB-C；R53/R54 靠近 STM32 PA12/PA11。
- 晶体、C68、C69 紧贴 MCU，且晶振回路下方不要走高速信号。
- 每个 100 nF 去耦必须紧贴对应电源脚，先到电容再到电源面，并使用短地过孔。
- WM8978 模拟输出远离 USB 和 I2S 时钟；耳机回流不要穿过 USB/MCU 数字回流区。
- SPK_P/SPK_N 按差分对成组走线，不允许任一路接地。
- USB 供电下扬声器满功率会显著增加 VBUS 电流，固件应限制初始音量并软启动。

## 校验结果

使用 KiCad 10.0.6 对层次原理图执行完整 ERC，错误和警告均为 0；同时导出网表
核对了 USB、I2S、控制总线、耳机和 BTL 扬声器的端到端连接。ERC 不能代替实物
验证，首板仍应依次检查 5 V、3.3 V、HSE、MCLK/BCLK/LRCLK、USB 枚举和静音
状态下的输出直流电压。
