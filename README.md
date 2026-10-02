# SAMD21E18A + WM8978 USB 声卡

正式工程位于 [pcb/WM8978.kicad_pro](pcb/WM8978.kicad_pro)。单页原理图使用 ATSAMD21E18A-A 与 WM8978，目标为 48 kHz / 16-bit 全双工 USB 音频：立体声耳机播放、单声道麦克风采集和 BTL 扬声器输出。项目目前只有硬件，没有固件或实板测试。

使用 KiCad 10.0 打开正式工程；项目自带的符号库和封装库采用相对路径，另需安装 KiCad 标准库。当前原理图的 9 处蓝色说明已改为中文，使用 KiCad 默认字体。可直接查看 [中文原理图 PDF](reports/project-snapshot/2026-09-23/WM8978.pdf) 和 [本次工程快照检查记录](reports/project-snapshot/2026-09-23/review.md)。

## 双时钟结构

| 器件 | 频率 | 去向 | 作用 |
| --- | --- | --- | --- |
| Y1 `ASE-12.000MHZ-LC-T` | 12.000 MHz | SAMD21 PA14/XIN | MCU 外部时钟参考；固件另行建立 USB 所需 48 MHz 时钟 |
| Y2 `ASE-12.288MHZ-L-C-T` | 12.288 MHz | 经 R73=33 Ω 到 WM8978 MCLK | 独立音频参考，直接得到 48 kHz 的 256fs 时钟 |

两个振荡器各有电源旁路电容（C103、C104），使能脚共接 PA18，并由 R74=100 kΩ 默认拉低。MCU 先用内部 OSC8M 启动，再使能振荡器；Y1 稳定后才能切换到 XOSC 外部时钟输入。Y1 与 Y2 的输出没有互连。

WM8978 作为 I²S 主机，输出 LRCLK/BCLK；SAMD21 的 I²S clock unit 0 配置为从机，由硬件串行器和 DMA 收发数据。48 kHz、双声道 32-bit 时隙时，BCLK 为 3.072 MHz。Codec 时钟寄存器 R6 应设置 `CLKSEL=0`（MCLK）、`MCLKDIV=000`（÷1）、`BCLKDIV=010`（÷4）、`MS=1`（主机），PLL 保持关闭。U1 的 PA07/PA08 为 I²S 发送/接收，PA10/PA11 接 codec 的 BCLK/LRCLK；PA14 只接 Y1，不接 codec MCLK。

USB 与音频时钟彼此独立。固件必须通过 USB Audio 异步反馈与 DMA 缓冲管理速率差，防止长期播放或录音时缓冲溢出、耗尽。挂起前先静音音频，把 MCU 切回内部时钟，再拉低 PA18 关闭 Y1/Y2。上电及停钟时的 WM8978 初始化/复位顺序需在固件中落实。

## 精简后的电路

原有 12 个测试点和仅为调试服务的 FAULT 上拉、手动复位按钮及其串联电阻已删除。WM8978 MODE 直接接地，AVDD 直接接 3.3 V。独立时钟方案进一步去掉了单门时钟缓冲器 U18 和它的输入下拉 R79，C104 改作 Y2 电源旁路；加入 Y2 后，正式原理图与 PCB 各有 **54 个实体器件**。

USB-C 两路 CC 下拉、D+/D− 串联电阻及 ESD、TPS2553 限流、3.3 V 稳压和各电源去耦继续承担功能。耳机左声道经 C87 到 CN2.4/Tip，右声道经 C86 到 CN2.3/Ring；扬声器接 WM8978 的 BTL 两端。四针 J2 保留 VTref、SWDIO、SWCLK、GND，已无外部 RESET 引脚；需要强制复位时可断电重上电。

`01_usb_power.kicad_sch`、`02_stm32.kicad_sch`、`03_wm8978.kicad_sch`、`04_audio_output.kicad_sch` 是旧方案的独立参考图，不属于正式工程的层次页。

封装已核对：14 个电阻和 25 个陶瓷电容均使用 0603 英制（1608 公制），原理图与 PCB 的封装标识一致。C86、C87 为 470 µF / 16 V 耳机输出耦合电解电容，保留 `CP_Elec_8x10.5` 封装。

2026-10-03 补齐了 C97～C100 的原理图符号与 PCB 封装关联。Y1/Y2 绑定 Abracon 原厂 STEP，MIC1 绑定立创原库 STEP，CN2 使用保留原库细节的五脚适配 STEP，并保存未经修改的六脚原文件。54 个器件的封装关联与 3D 模型路径均已验证；保留最新 PCB 布局、走线和电气连接。见 [封装与 3D 检查记录](reports/footprint-binding/2026-10-03/review.md)、[3D 预览](reports/footprint-binding/2026-10-03/pcb-3d.png)及[模型来源与适配说明](pcb/WM8978.3dshapes/README.md)。

## 检查与后续工作

2026-10-02 23:47 保存的 PCB 已完成连接，为 **54 个封装、252 段走线、285 个过孔、3 个铺铜/禁铺铜区**。C99 已移近主控 VDDANA 电源脚，R75/R76 已移近 codec 时钟输出端，U16 底部焊盘新增 9 个接地过孔。使用 KiCad 10.0.5 在内存中重新铺铜并运行原生 DRC：**0 个未连接项目、0 个原理图一致性问题**；当前规则下仍有 **402 个错误、3 个警告**。最新测量、层图及待处理事项见 [2026-10-02 23:47 PCB 复核](reports/pcb-review/2026-10-02-recheck-2347/review.md)。供电绕行、MCLK 路径及部分本地去耦仍有优化空间。

285 个过孔均为外径 0.50 mm / 孔径 0.30 mm、单边环宽 0.10 mm，[嘉立创制造工艺要求](https://www.jlc.com/portal/1/serviceGuide)支持双层板采用这一普通过孔尺寸。398 条过孔错误来自当前工程要求外径 ≥0.60 mm、环宽 ≥0.15 mm，与实际尺寸不符；这不等于该尺寸本身不能生产。工程规则尚未修改，需按最终选定板厂的工艺落实。其余为 USB2 封装内部 4 处孔距错误、CN2 丝印越板边 2 处警告和 C80/U16 丝印重叠 1 处警告；QFN 焊盘内过孔还需确认装配工艺。

原理图未改变，2026-10-02 原生 ERC 为 **0 错误、0 警告**，35 项关键拓扑检查通过，见 [初轮 PCB 复核](reports/pcb-review/2026-10-02/review.md)。此前 [2026-09-23 工程快照](reports/project-snapshot/2026-09-23/review.md)和双时钟复核记录保留作历史资料。

```bash
kicad-cli sch export netlist --format kicadxml -o reports/project-snapshot/2026-09-23/netlist.xml pcb/WM8978.kicad_sch
kicad-cli sch erc --severity-all --format json -o reports/project-snapshot/2026-09-23/erc.json pcb/WM8978.kicad_sch
python3 scripts/check_schematic.py --netlist reports/project-snapshot/2026-09-23/netlist.xml
kicad-cli pcb drc --severity-all --schematic-parity --format json -o reports/project-snapshot/2026-09-23/drc.json pcb/WM8978.kicad_pcb
```

固件需遵守 SAMD21 早期硅版本的 I²S 勘误约束，验证 USB 枚举/挂起、双向音频长时间稳定性、供电峰值和扬声器功耗。测试点删除后，首板仍可在器件焊盘或接口上测量主要信号，但调试便利性降低。

资料：[SAMD21 数据手册](https://ww1.microchip.com/downloads/aemDocuments/documents/MCU32/ProductDocuments/DataSheets/SAM-D21-DA1-Family-Data-Sheet-DS40001882.pdf)、[SAMD21 I²S 模式说明](https://developerhelp.microchip.com/xwiki/bin/view/products/mcu-mpu/32bit-mcu/sam/samd21-mcu-overview/peripherals/samd21-i2s-overview/)、[WM8978 数据手册](https://www.mouser.com/datasheet/2/76/WM8978_v4_5-1141768.pdf)、[Abracon ASE 系列](https://abracon.com/Oscillators/ASEseries.pdf)。
