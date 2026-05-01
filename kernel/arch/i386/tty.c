#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <string.h>

#include <kernel/tty.h>

#include "vga.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;
static uint16_t *const VGA_MEMORY = (uint16_t *)0xB8000;

void set_cursor_pos(size_t terminal_row, size_t terminal_column)
{
	size_t pos = terminal_row * VGA_WIDTH + terminal_column;
	__asm__ __volatile__("outb %%al, %%dx" : : "a"(0x0E), "d"(0x3D4));
	__asm__ __volatile__("outb %%al, %%dx"
			     :
			     : "a"((pos >> 8) & 0xFF), "d"(0x3D5));
	__asm__ __volatile__("outb %%al, %%dx" : : "a"(0x0F), "d"(0x3D4));
	__asm__ __volatile__("outb %%al, %%dx" : : "a"(pos & 0xFF), "d"(0x3D5));
}

static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t *terminal_buffer;

void terminal_initialize(void)
{
	terminal_row = 0;
	terminal_column = 0;
	terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
	terminal_buffer = VGA_MEMORY;

	for (size_t y = 0; y < VGA_HEIGHT; y++) {
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = y * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
	}
}

void terminal_setcolor(uint8_t color)
{
	terminal_color = color;
}

void terminal_putentryat(char c, uint8_t color, size_t x, size_t y)
{
	const size_t index = y * VGA_WIDTH + x;
	terminal_buffer[index] = vga_entry(c, color);
}

void increment_row()
{
	if (++terminal_row == VGA_HEIGHT) {
		for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
			for (size_t x = 0; x < VGA_WIDTH; x++) {
				const size_t index = y * VGA_WIDTH + x;
				terminal_buffer[index] = vga_entry(
				    terminal_buffer[index + VGA_WIDTH],
				    terminal_color);
			}
		}
		for (size_t x = 0; x < VGA_WIDTH; x++) {
			const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
			terminal_buffer[index] = vga_entry(' ', terminal_color);
		}
		terminal_row = VGA_HEIGHT - 1;
	}
}

void increment_column()
{
	if (++terminal_column == VGA_WIDTH) {
		increment_row();
		terminal_column = 0;
	}
}

void terminal_putchar(char c)
{
	if (c == '\n') {
		terminal_column = 0;
		increment_row();
	} else if (c == '\b') {
		if (terminal_column != 0) {
			terminal_column--;
			terminal_putentryat(' ', terminal_color,
					    terminal_column, terminal_row);
		}
	} else if (c == '\t') {
	} else {
		terminal_putentryat(c, terminal_color, terminal_column,
				    terminal_row);
		increment_column();
	}
	set_cursor_pos(terminal_row, terminal_column);
}

void terminal_write(const char *data, size_t size)
{
	for (size_t i = 0; i < size; i++)
		terminal_putchar(data[i]);
}

void terminal_writestring(const char *data)
{
	terminal_write(data, strlen(data));
}
