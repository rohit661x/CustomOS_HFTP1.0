
// obs/uart_complete.h (Slice 7)
#pragma once
#include <stdint.h>
void uart_putc(char c);
void uart_puts(const char* s);
void uart_put_hex(uint64_t v);
