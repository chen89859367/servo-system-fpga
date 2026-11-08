#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_tim.h"
#include "stm32f10x_usart.h"
#include "misc.h"

#include "aqbiss_sensor.h"
#include "console.h"
#include "main.h"

#define BISS_MA_PORT GPIOA
#define BISS_MA_PIN  GPIO_Pin_0
#define BISS_SL_PORT GPIOA
#define BISS_SL_PIN  GPIO_Pin_1

static uint16_t zero_offset = 0u;
static uint16_t last_raw = 0u;
static int32_t track_count = 0;
static uint8_t tracking_ready = 0u;

static uint16_t wrap_position_from_raw(uint16_t raw)
{
    return (uint16_t)(raw & (AQBISS_COUNTS_PER_REV - 1u));
}

static void report_read_error(int status, const aqbiss_frame_t *f)
{
    char buffer[128];
    if (status == -2) {
        snprintf(buffer, sizeof(buffer), "ENCODER ERROR E=%u POS=%u\n", f->error_bits, f->position);
    } else if (status == -3) {
        snprintf(buffer, sizeof(buffer), "CRC ERR RX=%02X CALC=%02X POS=%u\n", f->crc, f->crc_expected, f->position);
    } else {
        snprintf(buffer, sizeof(buffer), "READ ERR\n");
    }
    console_puts(buffer);
}

static void print_help(void)
{
    console_puts("Commands: B P T Sxxxx D U V Rxx Wxx yy F C G N H/? Z X\n");
    console_puts("B raw; P abs position; T unwrapped; S set zero; D clear; U sync; V show resolution\n");
    console_puts("R/W/F/C/G/N are intentionally unsupported in verified 15-bit sensor mode.\n");
}

static void handle_line(char *line)
{
    char cmd;
    char *arg;
    aqbiss_frame_t frame;
    int status;
    char buffer[128];

    while (*line == ' ') ++line;
    if (*line == '\0') return;

    cmd = (char)toupper((unsigned char)*line++);
    while (*line == ' ') ++line;
    arg = line;

    switch (cmd) {
    case 'B':
    case 'P':
        status = aqbiss_read(&frame);
        if (status != 0) {
            report_read_error(status, &frame);
            break;
        }
        if (frame.error_bits != 0u || !frame.crc_ok) {
            report_read_error((frame.error_bits != 0u) ? -2 : -3, &frame);
            break;
        }
        if (cmd == 'B') {
            snprintf(buffer, sizeof(buffer), "RAW=%u E=%u M=%u CRC=OK\n", frame.position, frame.error_bits, frame.m_bit);
        } else {
            snprintf(buffer, sizeof(buffer), "ABS=%u\n", wrap_position_from_raw((uint16_t)(frame.position - zero_offset)));
        }
        console_puts(buffer);
        break;

    case 'T': {
        int32_t delta;
        status = aqbiss_read(&frame);
        if (status != 0) {
            report_read_error(status, &frame);
            break;
        }
        if (frame.error_bits != 0u || !frame.crc_ok) {
            report_read_error((frame.error_bits != 0u) ? -2 : -3, &frame);
            break;
        }
        if (!tracking_ready) {
            last_raw = frame.position;
            track_count = (int32_t)frame.position;
            tracking_ready = 1u;
        } else {
            delta = (int32_t)frame.position - (int32_t)last_raw;
            if (delta > (int32_t)(AQBISS_COUNTS_PER_REV / 2u)) delta -= AQBISS_COUNTS_PER_REV;
            else if (delta < -(int32_t)(AQBISS_COUNTS_PER_REV / 2u)) delta += AQBISS_COUNTS_PER_REV;
            track_count += delta;
            last_raw = frame.position;
        }
        snprintf(buffer, sizeof(buffer), "TRK=%ld\n", (long)(track_count - zero_offset));
        console_puts(buffer);
        break;
    }

    case 'S': {
        char *end = NULL;
        long target = strtol(arg, &end, 10);
        if (target < 0 || target >= (long)AQBISS_COUNTS_PER_REV || end == arg) {
            console_puts("ERR use S<0..32767>\n");
            break;
        }
        status = aqbiss_read(&frame);
        if (status != 0) {
            report_read_error(status, &frame);
            break;
        }
        if (frame.error_bits != 0u || !frame.crc_ok) {
            report_read_error((frame.error_bits != 0u) ? -2 : -3, &frame);
            break;
        }
        zero_offset = wrap_position_from_raw((uint16_t)((int32_t)frame.position - (int32_t)target));
        snprintf(buffer, sizeof(buffer), "SET TO %ld OK (software offset)\n", target);
        console_puts(buffer);
        break;
    }

    case 'D':
        zero_offset = 0u;
        console_puts("ZERO CLEARED\n");
        break;

    case 'U':
        status = aqbiss_read(&frame);
        if (status != 0) {
            report_read_error(status, &frame);
            break;
        }
        if (frame.error_bits != 0u || !frame.crc_ok) {
            report_read_error((frame.error_bits != 0u) ? -2 : -3, &frame);
            break;
        }
        last_raw = frame.position;
        track_count = (int32_t)frame.position;
        tracking_ready = 1u;
        console_puts("SYNC OK\n");
        break;

    case 'V':
        console_puts("COUNTS=32768, NBITS=15, MODE=SENSOR\n");
        break;

    case 'R':
    case 'W':
        console_puts("NOT SUPPORTED: register-mode verified driver not present.\n");
        break;

    case 'F':
    case 'C':
    case 'G':
    case 'N':
        console_puts("NOT SUPPORTED: single 15-bit sensor channel\n");
        break;

    case 'H':
    case '?':
        print_help();
        break;

    case 'Z':
        console_puts("Netzer RE2 AqBiSS 15-bit - STM32F103ZC SPL port\n");
        break;

    case 'X':
        console_puts("RESET\n");
        NVIC_SystemReset();
        break;

    default:
        console_puts("ERR command; press H for help\n");
        break;
    }
}

void system_init(void)
{
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_AFIO, ENABLE);
    RCC_APB1PeriphClockCmd(RCC_APB1Periph_TIM2, ENABLE);
    aqbiss_init();
    console_init();
}

int main(void)
{
    char line[CMD_LINE_MAX];
    uint8_t used = 0u;

    system_init();
    console_puts("\nNetzer RE2 AqBiSS 15-bit STM32F103ZC\n115200-8-N-1\n0>");

    while (1) {
        int ch = console_getc();
        if (ch < 0) continue;
        if (ch == '\r' || ch == '\n') {
            console_putc('\n');
            line[used] = '\0';
            handle_line(line);
            used = 0u;
            console_puts("0>");
        } else if (ch == 0x08 || ch == 0x7F) {
            if (used > 0u) {
                --used;
                console_puts("\b \b");
            }
        } else if (ch >= 0x20 && ch <= 0x7E) {
            if (used < (CMD_LINE_MAX - 1u)) {
                line[used++] = (char)ch;
                console_putc((char)ch);
            } else {
                console_putc('\a');
            }
        }
    }
}
