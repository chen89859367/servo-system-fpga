#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "stm32f10x.h"
#include "stm32f10x_rcc.h"
#include "stm32f10x_gpio.h"
#include "stm32f10x_usart.h"
#include "aqbiss_sensor.h"

#define UART_BAUD 115200u
#define LINE_MAX 48u

static uint16_t zero_offset;
static uint16_t last_raw;
static int32_t track_count;
static uint8_t tracking_ready;

static void uart_init(void)
{
    GPIO_InitTypeDef gpio;
    USART_InitTypeDef uart;

    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA | RCC_APB2Periph_USART1, ENABLE);
    GPIO_StructInit(&gpio);
    gpio.GPIO_Pin = GPIO_Pin_9;
    gpio.GPIO_Speed = GPIO_Speed_50MHz;
    gpio.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_Init(GPIOA, &gpio);
    gpio.GPIO_Pin = GPIO_Pin_10;
    gpio.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOA, &gpio);

    USART_StructInit(&uart);
    uart.USART_BaudRate = UART_BAUD;
    uart.USART_WordLength = USART_WordLength_8b;
    uart.USART_StopBits = USART_StopBits_1;
    uart.USART_Parity = USART_Parity_No;
    uart.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    uart.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_Init(USART1, &uart);
    USART_Cmd(USART1, ENABLE);
}

static void putc_uart(char c)
{
    while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET) { }
    USART_SendData(USART1, (uint16_t)(uint8_t)c);
}

static void puts_uart(const char *s)
{
    while (*s != '\0') {
        if (*s == '\n') putc_uart('\r');
        putc_uart(*s++);
    }
}

static int getc_uart(void)
{
    if (USART_GetFlagStatus(USART1, USART_FLAG_RXNE) == RESET) return -1;
    return (int)(USART_ReceiveData(USART1) & 0xFFu);
}

static uint16_t mod_position(int32_t value)
{
    return (uint16_t)((uint32_t)value & (AQBISS_COUNTS_PER_REV - 1u));
}

static int read_position(aqbiss_frame_t *f)
{
    if (aqbiss_read(f) != 0) return -1;
    /* Do not silently use a frame with reported encoder errors or bad CRC. */
    if (f->error_bits != 0u) return -2;
    if (!f->crc_ok) return -3;
    return 0;
}

static void report_read_error(int status, const aqbiss_frame_t *f)
{
    char out[96];
    if (status == -2) {
        snprintf(out, sizeof(out), "ENCODER ERROR E=%u POS=%u\n", f->error_bits, f->position);
    } else if (status == -3) {
        snprintf(out, sizeof(out), "CRC ERR RX=%02X CALC=%02X POS=%u\n", f->crc, f->crc_expected, f->position);
    } else {
        snprintf(out, sizeof(out), "READ ERR\n");
    }
    puts_uart(out);
}

static void print_help(void)
{
    puts_uart("Commands: B P T Sxxxx D U V Rxx W xxyy F C G N H/? Z X\n");
    puts_uart("B raw; P zeroed single-turn; T unwrapped; S decimal set; D clear software zero; U sync; V resolution\n");
    puts_uart("R/W need verified register-mode support; F/C/G/N unsupported in 15-bit sensor mode.\n");
}

static int parse_hex_byte(const char *s, uint8_t *value)
{
    unsigned int v = 0u;
    uint8_t digits = 0u;
    while (*s == ' ') ++s;
    while (isxdigit((unsigned char)*s) && digits < 3u) {
        int c = toupper((unsigned char)*s++);
        v = (v << 4) | (unsigned int)(c <= '9' ? c - '0' : c - 'A' + 10);
        ++digits;
    }
    if (digits != 2u) return -1;
    while (*s == ' ') ++s;
    if (*s != '\0') return -1;
    *value = (uint8_t)v;
    return 0;
}

static void handle_line(char *line)
{
    char cmd;
    char out[128];
    aqbiss_frame_t f;
    int status;
    char *arg;

    while (*line == ' ') ++line;
    if (*line == '\0') return;
    cmd = (char)toupper((unsigned char)*line++);
    while (*line == ' ') ++line;
    arg = line;

    switch (cmd) {
    case 'B':
    case 'P':
        status = read_position(&f);
        if (status != 0) { report_read_error(status, &f); break; }
        if (cmd == 'B') {
            snprintf(out, sizeof(out), "RAW=%u (0x%04X) E=%u M=%u CRC=OK\n", f.position, f.position, f.error_bits, f.m_bit);
        } else {
            snprintf(out, sizeof(out), "ABS=%u\n", mod_position((int32_t)f.position - zero_offset));
        }
        puts_uart(out);
        break;
    case 'T': {
        int32_t delta;
        status = read_position(&f);
        if (status != 0) { report_read_error(status, &f); break; }
        if (!tracking_ready) {
            last_raw = f.position;
            track_count = f.position;
            tracking_ready = 1u;
        } else {
            delta = (int32_t)f.position - last_raw;
            if (delta > (int32_t)(AQBISS_COUNTS_PER_REV / 2u)) delta -= AQBISS_COUNTS_PER_REV;
            else if (delta < -(int32_t)(AQBISS_COUNTS_PER_REV / 2u)) delta += AQBISS_COUNTS_PER_REV;
            track_count += delta;
            last_raw = f.position;
        }
        snprintf(out, sizeof(out), "TRK=%ld\n", (long)(track_count - zero_offset));
        puts_uart(out);
        break;
    }
    case 'S': {
        char *end;
        long target = strtol(arg, &end, 10);
        while (*end == ' ') ++end;
        if (arg == end || *end != '\0' || target < 0 || target >= (long)AQBISS_COUNTS_PER_REV) {
            puts_uart("ERR use S<0..32767 decimal>\n");
            break;
        }
        status = read_position(&f);
        if (status != 0) { report_read_error(status, &f); break; }
        zero_offset = mod_position((int32_t)f.position - (int32_t)target);
        snprintf(out, sizeof(out), "SET TO %ld OK (software offset)\n", target);
        puts_uart(out);
        break;
    }
    case 'D':
        zero_offset = 0u;
        puts_uart("SOFTWARE ZERO CLEARED (encoder registers unchanged)\n");
        break;
    case 'U': {
        status = read_position(&f);
        if (status != 0) { report_read_error(status, &f); break; }
        last_raw = f.position;
        track_count = f.position;
        tracking_ready = 1u;
        puts_uart("TRACK SYNC OK\n");
        break;
    }
    case 'V':
        puts_uart("COUNTS=32768, NBITS=15, MODE=SENSOR\n");
        break;
    case 'R':
    case 'W':
        puts_uart("NOT SUPPORTED: register-mode driver/map not verified\n");
        break;
    case 'F': case 'C': case 'G': case 'N':
        puts_uart("NOT SUPPORTED: single 15-bit sensor channel\n");
        break;
    case 'H': case '?':
        print_help();
        break;
    case 'Z':
        puts_uart("Netzer RE2 AqBiSS 15-bit - STM32F103ZC SPL port\n");
        break;
    case 'X':
        puts_uart("System reset...\n");
        NVIC_SystemReset();
        break;
    default:
        /* Accept old Rxx and W xxyy command spellings without claiming support. */
        if (cmd == 'R' && *arg != '\0') {
            uint8_t address;
            if (parse_hex_byte(arg, &address) == 0) puts_uart("NOT SUPPORTED: register-mode driver/map not verified\n");
            else puts_uart("ERR command; press H for help\n");
        } else {
            puts_uart("ERR command; press H for help\n");
        }
        break;
    }
}

int main(void)
{
    char line[LINE_MAX];
    uint8_t used = 0u;

    SystemCoreClockUpdate();
    uart_init();
    aqbiss_init();
    puts_uart("\nNetzer RE2 AqBiSS 15-bit STM32F103ZC\n115200-8-N-1; type H for help\n0>");

    for (;;) {
        int ch = getc_uart();
        if (ch < 0) continue;
        if (ch == '\r' || ch == '\n') {
            putc_uart('\n');
            line[used] = '\0';
            handle_line(line);
            used = 0u;
            puts_uart("0>");
        } else if (ch == 0x08 || ch == 0x7F) {
            if (used > 0u) {
                --used;
                puts_uart("\b \b");
            }
        } else if (ch >= 0x20 && ch <= 0x7E) {
            if (used < (LINE_MAX - 1u)) {
                line[used++] = (char)ch;
                putc_uart((char)ch);
            } else {
                putc_uart('\a');
            }
        }
    }
}
