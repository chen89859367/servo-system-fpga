# Driver integration notes

This folder is reserved for board-specific and vendor library integration, such as:

- ST Standard Peripheral Library headers and source files
- CMSIS device headers and startup files
- project-specific linker scripts
- board configuration headers

For a real Keil or EWARM project, add the official STM32F10x SPL device package here and include:

- `stm32f10x.h`
- `stm32f10x_rcc.h`
- `stm32f10x_gpio.h`
- `stm32f10x_tim.h`
- `stm32f10x_usart.h`
- `misc.h`
- the correct startup assembly file for the selected STM32F103 device

This repository does not vendor the ST library sources, so the actual files are expected to be added from the STM32F10x SPL package or a vendor SDK.
