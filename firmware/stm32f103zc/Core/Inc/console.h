#ifndef __CONSOLE_H
#define __CONSOLE_H

#include <stdint.h>

void console_init(void);
void console_puts(const char *s);
void console_putc(char c);
int console_getc(void);

#endif
