#ifndef _KERNEL_TIMER_H
#define _KERNEL_TIMER_H

#include <stdint.h>

// Initialize PIT timer
void timer_init(uint32_t frequency);

// Get current tick count
uint32_t timer_get_ticks(void);

// Sleep for milliseconds
void timer_sleep(uint32_t ms);

// Timer interrupt handler
void timer_handler(void);

#endif