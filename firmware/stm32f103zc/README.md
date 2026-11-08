# STM32F103ZC AqBiSS Sensor-Mode Project

This directory is the engineering-structure version of the STM32F103ZC + Netzer RE2 AqBiSS port.

## Layout

```text
firmware/stm32f103zc/
├── README.md
├── .gitignore
├── Core/
│   ├── Inc/
│   │   ├── aqbiss_sensor.h
│   │   ├── console.h
│   │   └── main.h
│   └── Src/
│       ├── aqbiss_sensor.c
│       ├── console.c
│       └── main.c
├── Drivers/
│   └── README.md
├── Project/
│   └── README.md
└── Tools/
    └── README.md
```

## Purpose

- `Core` contains the application logic and the BiSS sensor capture engine.
- `Drivers` is the place for the vendor SPL or custom board support code.
- `Project` is reserved for the IDE-specific project files (Keil MDK / EWARM / Make).
- `Tools` can later store scripts, waveform capture helpers, and CRC verification tools.

## Build assumptions

- MCU: STM32F103ZC
- Toolchain: Keil MDK-ARM, IAR EWARM, or STM32CubeCLT Makeflow
- Library: ST Standard Peripheral Library (SPL) or equivalent CMSIS + CMSIS Device files
- Clock: 72 MHz system clock
- UART1: PA9 TX / PA10 RX at 115200-8-N-1
- SP490 RS-422 transceiver must be used between the encoder and the STM32 GPIOs

## Important notes

- This is a starter project structure, not a vendor-finalized project generation.
- Sensor-mode CRC init and exact timing must be verified against the RE2 Ug206 data and an oscilloscope.
- Register-mode commands (`R`, `W`, factory-reset operations) are intentionally left unsupported in the sensor-mode implementation until a verified register map and timing are available.
