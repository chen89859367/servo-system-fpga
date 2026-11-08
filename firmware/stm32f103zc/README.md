# STM32F103ZC AqBiSS RE2 sensor-mode port

This folder contains a first STM32 SPL firmware port for the Netzer RE2 configured for **15-bit, 32768-count single-turn AqBiSS sensor mode**. It replaces the C8051-specific SFR code and Fine/Coarse MN position calculation with direct sensor-frame capture.

## Files

- `main.c` — USART1 command line and legacy command names.
- `aqbiss_sensor.c/.h` — GPIO/Timer2 BiSS clocking, 25-bit sensor frame capture, position and CRC extraction.

## Hardware assumptions to verify before connecting power

- MCU: STM32F103ZC, SPL/CMSIS project, system clock configured to 72 MHz by the startup code.
- USART1: PA9 TX, PA10 RX, 115200 baud, 8-N-1, connected to FT232RL.
- MA clock GPIO: PA0, connected to the SP490 transmitter input (DI).
- SL data GPIO: PA1, connected to the SP490 receiver output (RO).
- The SP490 board's driver/receiver enables must be configured for clock output and data reception (`DE` active, `/RE` active-low). **Check the exact module schematic and signal polarity; do not connect encoder differential pairs directly to MCU GPIO.** This source does not assign SP490 enable pins because the module wiring was not supplied.
- Encoder and transceiver power, termination, grounding, and differential pair wiring must follow their datasheets.

Timer2 generates a nominal 1 MHz MA clock (1 us high + 1 us low). This is intentionally conservative for sensor mode and is not register mode. Verify the actual edge timing and sample edge with UG206 and an oscilloscope/logic analyzer on the assembled hardware.

## Frame layout implemented

The 25 sampled bits are treated as:

```text
[0] Start
[1..15] Position S14..S0 (MSB first)
[16..17] E bits
[18..22] CRC C4..C0
[23] M
[24] Stop
```

The source extracts these fields and reports E/M/CRC status. It does not interpret Start/Stop logic levels because those exact levels/timing were not provided in the repository conversation; check them against the encoder's UG206 revision.

CRC is implemented MSB-first with width 5 and lower polynomial bits `0x05` (polynomial x^5+x^2+1), followed by inversion (`^ 0x1F`). **The CRC initial value is a configurable placeholder (`BISS_CRC_INIT`, currently 0); confirm it against the exact UG206 specification before relying on `CRC=OK`.** If the initialization differs, change the macro in `aqbiss_sensor.h`. Do not treat this initial value as verified.

## Commands

Commands are line-oriented, case-insensitive, and accept the old compact forms and spaced forms where applicable:

- `B` — read position and print decimal count plus frame status.
- `P` — print single-turn absolute position after the software zero offset.
- `T` — print software-unwrapped tracking position.
- `S1234` or `S 1234` — set the current position to decimal 1234 (software offset only).
- `D` — clear the software zero offset; this does **not** reset encoder registers/factory settings.
- `U` — synchronize tracking to the current position.
- `V` — display 32768 counts / 15 bits.
- `Rxx` / `R xx` and `W xxyy` / `W xx yy` — retained command spellings, return `NOT SUPPORTED` in sensor mode. Register access needs a separate, verified AqBiSS register-mode implementation at no more than 250 kHz and the correct register map.
- `F`, `C`, `G`, `N` — retained spellings, return `NOT SUPPORTED` because this encoder is configured for one 15-bit position channel; no legacy Fine/Coarse remapping is performed.
- `H` / `?`, `Z`, `X` — help, version, MCU reset.

## Integration/build

Add these C files to an STM32F10x SPL project and include the matching vendor headers/libraries (`stm32f10x.h`, GPIO, RCC, TIM, USART, CMSIS startup/system files). Ensure the startup/system code configures a 72 MHz system clock before `main`. The code is a source drop, not a complete Keil/IAR/Make project; the repository currently contains no STM32 project template. Build with warnings enabled and confirm the clock waveform, frame alignment, CRC initialization and encoder responses on hardware before using position values for motion control.

Legacy commands `R/W` and factory-reset semantics are not falsely emulated: the sensor-mode implementation explicitly reports them unsupported until the RE2 register protocol and writable parameters are verified.
