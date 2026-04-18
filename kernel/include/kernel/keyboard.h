#ifndef _KERNEL_KEYBOARD_H
#define _KERNEL_KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>

// Initialize keyboard driver
void keyboard_init(void);

// Check if a key is available
bool keyboard_has_key(void);

// Get a key (blocking)
char keyboard_getchar(void);

// Get raw scancode
uint8_t keyboard_get_scancode(void);

// Keyboard interrupt handler
void keyboard_handler(void);

#endif