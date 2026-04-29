#ifndef _KERNEL_KBD_H
#define _KERNEL_KBD_H

#include <stdbool.h>

void kbd(void);
void kbd_init(void);

bool kbd_has_key(void);
char kbd_getchar(void);

#endif
