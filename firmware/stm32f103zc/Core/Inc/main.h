#ifndef __MAIN_H
#define __MAIN_H

#include "stm32f10x.h"
#include "aqbiss_sensor.h"

#define UART_BAUDRATE 115200u
#define CMD_LINE_MAX  64u

void system_init(void);

#endif
