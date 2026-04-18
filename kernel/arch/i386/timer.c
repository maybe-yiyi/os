#include <stdint.h>
#include <kernel/timer.h>
#include <kernel/apic.h>

// PIT (Programmable Interval Timer) ports
#define PIT_COMMAND 0x43
#define PIT_CHANNEL0 0x40
#define PIT_CHANNEL1 0x41
#define PIT_CHANNEL2 0x42

// PIT frequency
#define PIT_FREQUENCY 1193182

static volatile uint32_t ticks = 0;
static uint32_t frequency = 0;

// Send command to PIT
static void pit_send_command(uint8_t cmd) {
    __asm__ __volatile__ ("outb %0, %1" : : "a"(cmd), "Nd"(PIT_COMMAND));
}

// Set PIT frequency
static void pit_set_frequency(uint32_t freq) {
    uint32_t divisor = PIT_FREQUENCY / freq;
    pit_send_command(0x36); // Command: channel 0, lobyte/hibyte, mode 3
    __asm__ __volatile__ ("outb %0, %1" : : "a"((uint8_t)(divisor & 0xFF)), "Nd"(PIT_CHANNEL0));
    __asm__ __volatile__ ("outb %0, %1" : : "a"((uint8_t)((divisor >> 8) & 0xFF)), "Nd"(PIT_CHANNEL0));
}

void timer_init(uint32_t freq) {
    frequency = freq;
    ticks = 0;
    pit_set_frequency(freq);
}

void timer_handler(void) {
    ticks++;
}

uint32_t timer_get_ticks(void) {
    return ticks;
}

void timer_sleep(uint32_t ms) {
    uint32_t target = ticks + (ms * frequency) / 1000;
    while (ticks < target) {
        __asm__ __volatile__ ("sti; hlt; cli");
    }
}