# USB D+/D- rework for the assembled WM8978 board

## Confirmed PCB error

STM32F407VET6 has fixed USB OTG FS pins:

| Signal | STM32 pin |
|---|---|
| USB D- | PA11, package pin 70 |
| USB D+ | PA12, package pin 71 |

The current Type-C routing is reversed:

| Type-C contacts | Actual USB signal | Current destination | Correct destination |
|---|---|---|---|
| A6 and B6 | D+ | PA11 | PA12 |
| A7 and B7 | D- | PA12 | PA11 |

Both plug orientations are affected. Turning the Type-C plug over cannot fix
it, and the STM32 USB peripheral cannot swap these pins in software.

## Rework on the current PCB

Power the board off and disconnect USB and J-Link before cutting traces.

Near the Type-C connector, the two merged USB tracks leave the connector as
two close vertical tracks in the PCB coordinate area around Y=68..71 mm:

- the track at approximately X=150.123 mm is connector D+ but is presently
  named `PA11`;
- the track at approximately X=150.623 mm is connector D- but is presently
  named `PA12`.

1. Cut both tracks at a clear point after the A/B contacts of each signal have
   merged, but before they route toward the STM32.
2. Check with a multimeter that each cut is open.
3. Cross the two cuts with insulated enamel wire:
   - connector-side D+ (X≈150.123) to MCU-side PA12 (X≈150.623);
   - connector-side D- (X≈150.623) to MCU-side PA11 (X≈150.123).
4. Check that D+ and D- are not shorted to each other, GND, or VBUS.
5. Keep both jumpers short and similar in length. Do not create large loops.

If cutting these tracks is inconvenient, an equivalent repair is to isolate
PA11 and PA12 close to the MCU and cross-connect the two nets there. Avoid
lifting adjacent LQFP pins unless you have suitable rework equipment.

## Verification

After flashing the firmware and reconnecting USB, Linux should report a
**full-speed** device rather than a low-speed device:

```sh
lsusb | grep -i 'cafe:4010'
cat /proc/asound/cards
```

Expected USB identity:

```text
VID:PID  cafe:4010
Product  WM8978 USB Audio
Class    USB Audio Class 1.0
Format   48 kHz, signed 16-bit little-endian, stereo
```

If D+/D- are fixed but enumeration remains intermittent, repair the X3 8 MHz
HSE oscillator path (X3, C68, C69 and their solder joints). Diagnostics word 3
at `0x2001FF0C` is `1` when HSE works and `2` when firmware is using the less
accurate internal-HSI fallback.
