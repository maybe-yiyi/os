#ifndef _KERNEL_APIC_H
#define _KERNEL_APIC_H

#include <stdint.h>

extern uint32_t APIC_BASE_ADDRESS;

void enable_apic(void);

#endif
