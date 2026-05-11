#ifndef _KERNEL_PANIC_H
#define _KERNEL_PANIC_H

void kernel_panic(const char *restrict err);
void kernel_panicf(const char *restrict format, ...) __attribute__ ((format(printf, 1, 2)));

#endif
