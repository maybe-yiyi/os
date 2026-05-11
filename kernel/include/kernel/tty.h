#ifndef _KERNEL_TTY_H
#define _KERNEL_TTY_H

#include <stddef.h>

/**
 * Initialize the terminal to be empty.
 */
void terminal_initialize(void);

/**
 * Writes a character to the terminal.
 */
void terminal_putchar(char c);

/**
 * Writes a char* with size to the terminal.
 */
void terminal_write(const char *restrict data, size_t size);

/**
 * Writes a string to the terminal.
 */
void terminal_writestring(const char *restrict data);

#endif
