#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

#include <kernel/kbd.h>

#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_COMMAND_PORT 0x64

#define KEYBOARD_BUFFER_SIZE 256

static volatile uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t buffer_head = 0;
static volatile uint32_t buffer_tail = 0;

static const char scancode_to_ascii[] = {
    0,	  27,	'1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-',	'=',
    '\b', '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[',	']',
    '\n', 0,	'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0,	  '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,	'*',
    0,	  ' ',	0,   0,	  0,   0,   0,	 0,   0,   0,	0,   0,	  0,	0,
    0,	  0,	0,   0,	  0,   0,   0,	 0,   0,   0,	0,   0,	  0,	0,
    0,	  0,	0,   0,	  0,   '-', 0,	 0,   0,   '+', 0,   0,	  0,	0,
    0,	  0,	0,   0,	  0,   0,   0};

static const char scancode_to_ascii_shift[] = {
    0,	  27,	'!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+',
    '\b', '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}',
    '\n', 0,	'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0,	  '|',	'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,   '*',
    0,	  ' ',	0,   0,	  0,   0,   0,	 0,   0,   0,	0,   0,	  0,   0,
    0,	  0,	0,   0,	  0,   0,   0,	 0,   0,   0,	0,   0,	  0,   0,
    0,	  0,	0,   0,	  0,   '-', 0,	 0,   0,   '+', 0,   0,	  0,   0,
    0,	  0,	0,   0,	  0,   0,   0};

static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock = false;

static uint8_t kbd_read_status(void)
{
	uint8_t status;
	__asm__ __volatile__("inb %1, %0"
			     : "=a"(status)
			     : "Nd"(KEYBOARD_STATUS_PORT));
	return status;
}

static uint8_t kbd_read_data(void)
{
	uint8_t data;
	__asm__ __volatile__("inb %1, %0"
			     : "=a"(data)
			     : "Nd"(KEYBOARD_DATA_PORT));
	return data;
}

static void kbd_wait(void)
{
	while (kbd_read_status() & 0x02)
		;
}

static void kbd_send_command(uint8_t cmd)
{
	kbd_wait();
	__asm__ __volatile__("outb %0, %1"
			     :
			     : "a"(cmd), "Nd"(KEYBOARD_COMMAND_PORT));
}

static void kbd_buffer_put(char c)
{
	uint32_t next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
	if (next_head != buffer_tail) {
		keyboard_buffer[buffer_head] = (uint8_t)c;
		buffer_head = next_head;
	}
}

void kbd(void)
{
	uint8_t scancode = kbd_read_data();

	if (scancode & 0x80) {
		scancode &= 0x7F;
		switch (scancode) {
		case 0x2A:
		case 0x36: // Shift
			shift_pressed = false;
			break;
		case 0x1D: // Ctrl
			ctrl_pressed = false;
			break;
		case 0x38: // Alt
			alt_pressed = false;
			break;
		}
	} else {
		// Key pressed
		switch (scancode) {
		case 0x2A:
		case 0x36: // Shift
			shift_pressed = true;
			break;
		case 0x1D: // Ctrl
			ctrl_pressed = true;
			break;
		case 0x38: // Alt
			alt_pressed = true;
			break;
		case 0x3A: // Caps Lock
			caps_lock = !caps_lock;
			break;
		default:
			if (scancode < sizeof(scancode_to_ascii)) {
				char c = shift_pressed
					     ? scancode_to_ascii_shift[scancode]
					     : scancode_to_ascii[scancode];
				if (c) {
					// Handle caps lock for letters
					if (caps_lock && c >= 'a' && c <= 'z') {
						c = c - 'a' + 'A';
					} else if (caps_lock && c >= 'A' &&
						   c <= 'Z') {
						c = c - 'A' + 'a';
					}

					// Handle Ctrl combinations
					if (ctrl_pressed) {
						if (c >= 'a' && c <= 'z') {
							c = c - 'a' +
							    1; // Ctrl+A = 1,
							       // etc.
						} else if (c >= 'A' &&
							   c <= 'Z') {
							c = c - 'A' + 1;
						}
					}

					kbd_buffer_put(c);
				}
			}
			break;
		}
	}
}

void kbd_init(void)
{
	buffer_head = 0;
	shift_pressed = false;
	buffer_tail = 0;
	ctrl_pressed = false;
	alt_pressed = false;
	caps_lock = false;

	while (kbd_read_status() & 0x01) {
		kbd_read_data();
	}

	// enable keyboard
	kbd_send_command(0xAE);
}

bool kbd_has_key(void)
{
	return buffer_head != buffer_tail;
}

char kbd_getchar(void)
{
	while (buffer_head == buffer_tail) {
		// Wait for key
		__asm__ __volatile__("sti; hlt; cli");
	}

	char c = (char)keyboard_buffer[buffer_tail];
	buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
	return c;
}
