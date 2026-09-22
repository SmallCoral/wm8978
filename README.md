# SAMD21E18A + WM8978 USB 声卡

**最新复查：当前不能直接 layout。** 后续调整中，时钟源侧 `I2S_MCLK` 标签缺失，WM8978 MCLK 断网；本次原生 ERC 为 **1 错误、1 警告**。下载接口现为四针 J2（1=3V3、2=SWDIO、3=SWCLK、4=GND）。当前问题、改进建议及文件哈希见 [最新复查记录](reports/schematic-review/recheck-current.md)。下文旧 J1 和“200 项通过”等完成记录对应此前版本。

当前正式原理图使用 **ATSAMD21E18A-A + WM8978**，目标为 48 kHz / 16-bit USB 全双工音频：立体声播放、单声道麦克风采集、耳机输出和 BTL 扬声器输出。工程仅包含硬件，尚无固件或实板验证。

## 打开工程

使用 KiCad 10 打开 `pcb/WM8978.kicad_pro`，以 `pcb/WM8978.kicad_sch` 为正式设计。正式图保留 A3 单页，分为 USB/电源、SAMD21 主控和 WM8978/音频三个功能区。

**2026-09-22 的修改仅针对原理图。PCB 没有更新，仍属于旧 STM32 方案，不能用作当前 SAMD21 方案的生产文件。** 项目设置文件保留了用户原有改动。

`01_usb_power.kicad_sch`、`02_stm32.kicad_sch`、`03_wm8978.kicad_sch`、`04_audio_output.kicad_sch` 是旧方案的独立参考副本，未作为层次页引用，本次未同步。

## 主控接线

| U1 引脚 | 功能 | 网络 / 对端 |
| --- | --- | --- |
| 8 / PA07 | I²S SD0，发送 | I2S_DACDAT → U16.10 |
| 11 / PA08 | I²S SD1，接收 | I2S_ADCDAT ← U16.9 |
| 13 / PA10 | I²S SCK0，输入 | I2S_BCLK ← U16.8 |
| 14 / PA11 | I²S FS0，输入 | I2S_LRCLK ← U16.7 |
| 15 / PA14 | XIN，外部时钟输入 | I2S_MCLK，12 MHz |
| 19 / PA18 | 时钟使能 GPIO | AUDIO_CLK_EN → Y1.1 |
| 21 / PA22 | SERCOM3 PAD0，SDA | CODEC_SDA ↔ U16.17 |
| 22 / PA23 | SERCOM3 PAD1，SCL | CODEC_SCL → U16.16 |
| 23 / PA24 | USB D− | USB_DM，经 R54 接 USB-C |
| 24 / PA25 | USB D+ | USB_DP，经 R53 接 USB-C |
| 26 / RESET | 低有效复位 | MCU_RESET_N |
| 31 / PA30 | SWCLK | J1.4，R71=1 kΩ 上拉 |
| 32 / PA31 | SWDIO | J1.2 |
| 9 / VDDANA、30 / VDDIN | 3.3 V 输入 | 各有 100 nF + 10 µF 去耦 |
| 29 / VDDCORE | 内核稳压器输出 | 仅接 C101=1 µF 到 GND |
| 10、28 / GND | 接地 | GND |

未使用的 GPIO 已明确标记 NC。VDDCORE 不接 3V3，也不用作外设电源。

## 时钟、复位与下载

- Y1：Abracon `ASE-12.000MHZ-LC-T`，3.3 V / 12 MHz / ±50 ppm，C103=10 nF 去耦。
- Y1 输出经 R73=33 Ω，同时送 MCU PA14/XIN 和 WM8978 MCLK。
- R74=100 kΩ 将时钟使能默认拉低；MCU 先从内部 OSC8M 启动，再拉高 PA18，等待至少 2 ms 后启用 XOSC 外部时钟模式。PA15/XOUT 不使用。
- WM8978 使用自身 PLL 生成音频时钟并输出 BCLK/LRCLK；SAMD21 两个 serializer 共用 I²S clock unit 0，作为从机收发。
- 48 kHz PLL 参考参数：N=8、K=0x3126E9、PLLPRESCALE=0、MCLKDIV=/2。
- 复位使用 R70=10 kΩ 上拉、C102=100 nF 滤波，SW1 通过 R72=330 Ω 拉低复位。
- J1 为 2×5、1.27 mm Cortex SWD 针序：1=3V3/VTref、2=SWDIO、3/5/9=GND、4=SWCLK、10=RESET，6/7/8 不接。下载器使用目标电压参考，不从此口接入 5 V。

## 已完成的修正与复核

- 接通 `MICBIAS`：U16.32、C96.1、R68.1 现在属于同一网络，修复麦克风偏置断线。
- 补全主控 USB、I²S、I²C、供电、时钟、复位和调试连接，共新增 15 个器件。
- USB-C 两路 CC 各自通过 5.1 kΩ 下拉；D+/D− 的双面触点、22 Ω 串联电阻和对地 TVS 网络正确。
- 保留 TLV1117LV33DCYR 和现有电容，不应未经稳定性核查替换成普通 AMS1117。
- WM8978 的 DCVDD、DBVDD、AVDD 接 3V3，SPKVDD 接 VBUS_5V；地脚与裸露焊盘均接 GND。
- MODE 下拉、CSB 接地，使用两线控制，7-bit 地址 0x1A。固件须保持 GPIO1/CSB 为输入。
- 耳机 C86/C87 正极朝向芯片；SPK2 两端接 ROUT2/LOUT2，均不接地。
- 修正旧 STM32 图签，添加模块说明，整理密集元件文字。

## 检查结果

KiCad **10.0.5 原生 ERC：0 错误、0 警告**。另完成 200 项网表/参数检查，其中包含修改前全部非主控引脚连接的逐项比较：唯一合并的原有网络是修复的 MICBIAS。PCB 的 SHA-256 与修改前一致。

**Layout 复核尚未放行：** CN2 封装缺少所附图纸要求的定位孔；USB2、耳机电容和 codec 封装仍需按实际料号确认。还需明确 SAMD21 硅版本的 I²S 勘误约束、USB 上电浪涌和 Y1 双负载驱动余量。详见 [layout 前复核记录](reports/schematic-review/layout-readiness.md)。

- `reports/schematic-review/WM8978.svg`：KiCad 原生导出的整张图。
- `reports/schematic-review/erc.json`：原生 ERC 报告。
- `reports/schematic-review/netlist.xml`：原生网表。
- `reports/schematic-review/connectivity-check.json`：拓扑和 PCB 校验记录。
- `reports/schematic-review/review.md`：设计复核和固件约束。
- `reports/schematic-review/layout-readiness.md`：layout 前待解决事项与设计改进建议。
- `reports/schematic-review/layout-pad-check.json`：48 个器件的符号引脚/封装焊盘编号核对。

重新执行检查：

```powershell
kicad-cli sch export netlist --format kicadxml -o reports/schematic-review/netlist.xml pcb/WM8978.kicad_sch
kicad-cli sch erc --format json --severity-all --exit-code-violations -o reports/schematic-review/erc.json pcb/WM8978.kicad_sch
python scripts/check_schematic.py
```

## 后续验证

固件需要实现 48 MHz USB 时钟及 pad 校准、codec 初始化、I²S DMA、USB 音频反馈/缓冲管理、BTL 混音和反相配置。板上时钟与 USB 主机异步，不能省略速率匹配。枚举前保持扬声器关闭，按主机允许电流限制功率；挂起时先关闭音频，把 MCU 切到内部时钟，再关闭 Y1。

ERC 和网表通过不代表整机已经实测。首板需验证时钟、USB 枚举及挂起电流、音频连续收发、噪声和输出功率。现有 USB 屏蔽层直连地、VBUS 无独立 TVS 的做法，应结合外壳和 EMC/ESD 目标验证。

## 依据

- [Microchip SAM D21 数据手册](https://ww1.microchip.com/downloads/en/DeviceDoc/SAM-D21DA1-Family-Data-Sheet-DS40001882G.pdf)：引脚复用、I²S、供电及原理图检查。
- [Wolfson WM8978 Rev 4.5 数据手册](https://www.mouser.com/datasheet/2/76/WM8978_v4_5-1141768.pdf)：时钟、两线控制及音频输出。
- [Abracon ASE 数据手册](https://abracon.com/Oscillators/ASEseries.pdf)：振荡器规格和使能。
- [TI TLV1117LV 数据手册](https://www.ti.com/lit/ds/symlink/tlv1117lv.pdf)。
- [Nexperia PESD5V0U1BA 数据手册](https://assets.nexperia.com/documents/data-sheet/PESD5V0U1BA.pdf)。
