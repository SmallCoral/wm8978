# WM8978 USB Audio firmware

Bare-metal firmware for this repository's STM32F407VET6 + WM8978 board. It
enumerates as a driverless USB Audio Class 1.0 playback device named
`WM8978 USB Audio` and sends 48 kHz, 16-bit stereo samples to the headphone
outputs through I2S2 and DMA.

## Required USB hardware rework for the current PCB

The assembled PCB cannot enumerate correctly until USB D+ and D- are crossed.
The Type-C receptacle currently routes A6/B6 (D+) to STM32 PA11/USB_DM and
A7/B7 (D-) to PA12/USB_DP. This is reversed. The Linux host consequently sees
a false low-speed attach followed by `error -32` / `error -71`.

See [`USB_REWORK.md`](USB_REWORK.md) for the exact cut-and-jumper procedure.
This cannot be corrected by STM32 firmware because the OTG FS peripheral has
fixed analog D+/D- pins.

## Signal assignment

| Function | STM32F407 pin | WM8978 pin/net |
|---|---:|---|
| USB D- / D+ | PA11 / PA12 | USB-C connector |
| Control clock / data | PD6 / PD7 | SCLK / SDIN (2-wire mode) |
| I2S word clock | PB12 | LRC |
| I2S bit clock | PB13 | BCLK |
| I2S playback data | PC3 | DACDAT |
| Codec master clock | PC6 net, high-Z | external 24.576 MHz oscillator |
| Status LED | PC0 | LED4, active low |

The codec is the audio-clock master. Its 24.576 MHz oscillator is divided to
12.288 MHz SYSCLK, 3.072 MHz BCLK, and 48 kHz LRCLK. The USB asynchronous
feedback endpoint follows the codec clock so the host and codec cannot slowly
drift apart.

## Build and flash

Requirements: `arm-none-eabi-gcc`, GNU Make, and OpenOCD with libjaylink.

```sh
make
make flash
```

OpenOCD is deliberately used instead of SEGGER J-Link Commander. It supports
this probe without displaying the J-Link clone warning dialog.

If J-Link cannot attach because the previous probe firmware is sleeping, run:

```sh
./recover_after_reset.sh
```

and press the board reset button once while dots are being printed. All J-Link
access in the normal build and recovery flow goes through OpenOCD, so SEGGER's
warning GUI is not started.

## Runtime diagnostics

`make test` reads words starting at SRAM address `0x2001FF00`:

| Word | Meaning |
|---:|---|
| 0 | magic `0x55414331` (`UAC1`) |
| 1 | firmware version |
| 2 | stage: 5 means USB stack running; `0xE001..4` are errors |
| 3 | clock source: 1 = 8 MHz HSE, 2 = internal-HSI fallback |
| 4 | WM8978 acknowledged all setup writes |
| 5 | I2S2 DMA started |
| 6 | USB mounted/configured by the host |
| 7 | host selected the streaming interface |
| 8-9 | DMA half/full interrupt counts |
| 10 | audio underrun count |
| 11 | USB audio bytes received |
| 12 | sample rate (48000) |
| 13 | current volume in signed 1/256 dB |
| 14 | mute state |
| 15 | current TinyUSB audio FIFO byte count |
| 16 | measured 24.576 MHz codec MCLK frequency |
| 17 | measured codec LRCLK frequency (normally 48000) |
| 18 | audio clock mode: 1 = X4/codec master, 2 = STM32 fallback master |

LED4 blinks quickly before enumeration, slowly when mounted, and rapidly while
audio streaming. The initial codec gain is -12 dB to make first testing safer.

The normal clock source is the board's 8 MHz crystal (168 MHz CPU and exact
48 MHz USB). If that crystal does not start, the firmware records value 2 and
uses a nominal 48 MHz USB clock derived from HSI so assembly testing can
continue; HSE should still be repaired for production-grade USB tolerance.

The firmware also detects the separate X4 24.576 MHz audio oscillator. If X4
does not run, it switches WM8978 into clock-slave mode and generates MCLK,
BCLK, and LRCLK from STM32 PLLI2S (diagnostic clock mode 2). The measurement
uses a 1 ms gate because TIM3 is a 16-bit counter on STM32F407.

Third-party source code is vendored under `third_party/`: TinyUSB 0.21.0,
CMSIS Device F4 2.6.11, and CMSIS 5.9.0. Their licenses are included beside
the sources.
