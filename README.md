# WM8978 USB 声卡

基于 **ATSAMD21E18A** 和 **WM8978** 的 USB 音频硬件项目，目标支持 **48 kHz / 16-bit 全双工音频**，提供耳机播放、麦克风采集和扬声器输出。

![PCB 3D 预览](./docs/images/pcb-3d-preview.png)

## 硬件特点

- USB-C 供电与通信，包含 ESD 保护、限流和 3.3 V 稳压。
- SAMD21 与 WM8978 通过 I²S 传输音频，通过 I²C 配置 codec。
- 独立的 12 MHz 主控时钟与 12.288 MHz 音频时钟。
- 立体声耳机输出、驻极体麦克风输入、BTL 扬声器输出及 SWD 调试接口。
- 提供 KiCad 原理图、PCB、自定义库和项目 3D 模型。

## 打开工程

安装 **KiCad 10** 及其标准符号、封装和 3D 模型库，然后打开 [WM8978.kicad_pro](pcb/WM8978.kicad_pro)。项目自定义库与模型使用相对路径。

## 目录

| 目录 | 内容 |
| --- | --- |
| [pcb/](pcb/) | KiCad 工程、原理图、PCB 与器件库 |
| [scripts/](scripts/) | 原理图检查、PCB 同步与模型处理脚本 |
| [reports/](reports/) | 设计复核记录与导出资料 |

## 项目状态

目前处于硬件验证阶段，USB 音频固件与实板测试尚未完成。

[原理图预览](reports/project-snapshot/2026-09-23/WM8978.pdf) · [设计检查记录](reports/footprint-binding/2026-10-03/review.md) · [3D 模型说明](pcb/WM8978.3dshapes/README.md)
