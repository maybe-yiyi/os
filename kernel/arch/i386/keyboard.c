#include <stdint.h>
#include <stdbool.h>
#include <kernel/keyboard.h>
#include <kernel/idt.h>

// Keyboard ports
#define KEYBOARD_DATA_PORT 0x60
#define KEYBOARD_STATUS_PORT 0x64
#define KEYBOARD_COMMAND_PORT 0x64

// Circular buffer for keyboard input
#define KEYBOARD_BUFFER_SIZE 256

static volatile uint8_t keyboard_buffer[KEYBOARD_BUFFER_SIZE];
static volatile uint32_t buffer_head = 0;
static volatile uint32_t buffer_tail = 0;

// US Keyboard scancode to ASCII mapping (scancode set 1)
static const char scancode_to_ascii[] = {
    0, 27, '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
    '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
    0, 'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
    0, '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static const char scancode_to_ascii_shift[] = {
    0, 27, '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
    '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
    0, 'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
    0, '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
    '*', 0, ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '-', 0, 0, 0, '+',
    0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

static bool shift_pressed = false;
static bool ctrl_pressed = false;
static bool alt_pressed = false;
static bool caps_lock = false;

// Read keyboard status
static uint8_t keyboard_read_status(void) {
    uint8_t status;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(status) : "Nd"(KEYBOARD_STATUS_PORT));
    return status;
}

// Read keyboard data
static uint8_t keyboard_read_data(void) {
    uint8_t data;
    __asm__ __volatile__ ("inb %1, %0" : "=a"(data) : "Nd"(KEYBOARD_DATA_PORT));
    return data;
}

// Send command to keyboard
static void keyboard_send_command(uint8_t cmd) {
    while (keyboard_read_status() & 0x02); // Wait for buffer
    __asm__ __volatile__ ("outb %0, %1" : : "a"(cmd), "Nd"(KEYBOARD_COMMAND_PORT));
}

// Wait for keyboard to be ready
static void keyboard_wait(void) {
    while (keyboard_read_status() & 0x02);
}

// Add character to buffer
static void keyboard_buffer_put(char c) {
    uint32_t next_head = (buffer_head + 1) % KEYBOARD_BUFFER_SIZE;
    if (next_head != buffer_tail) {
        keyboard_buffer[buffer_head] = (uint8_t)c;
        buffer_head = next_head;
    }
}

void keyboard_handler(void) {
    uint8_t scancode = keyboard_read_data();
    
    // Check if key released (bit 7 set)
    if (scancode & 0x80) {
        scancode &= 0x7F;
        switch (scancode) {
            case 0x2A: case 0x36: // Shift
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
            case 0x2A: case 0x36: // Shift
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
                    char c = shift_pressed ? scancode_to_ascii_shift[scancode] 
                                           : scancode_to_ascii[scancode];
                    if (c) {
                        // Handle caps lock for letters
                        if (caps_lock && c >= 'a' && c <= 'z') {
                            c = c - 'a' + 'A';
                        } else if (caps_lock && c >= 'A' && c <= 'Z') {
                            c = c - 'A' + 'a';
                        }
                        
                        // Handle Ctrl combinations
                        if (ctrl_pressed) {
                            if (c >= 'a' && c <= 'z') {
                                c = c - 'a' + 1; // Ctrl+A = 1, etc.
                            } else if (c >= 'A' && c <= 'Z') {
                                c = c - 'A' + 1;
                            }
                        }
                        
                        keyboard_buffer_put(c);
                    }
                }
                break;
        }
    }
}

void keyboard_init(void) {
    buffer_head = 0;
    buffer_tail = 0;
    shift_pressed = false;
    ctrl_pressed = false;
    alt_pressed = false;
    caps_lock = false;
    
    // Empty keyboard buffer
    while (keyboard_read_status() & 0x01) {
        keyboard_read_data();
    }
    
    // Enable keyboard
    keyboard_send_command(0xAE);
}

bool keyboard_has_key(void) {
    return buffer_head != buffer_tail;
}

char keyboard_getchar(void) {
    while (buffer_head == buffer_tail) {
        // Wait for key
        __asm__ __volatile__ ("sti; hlt; cli");
    }
    
    char c = (char)keyboard_buffer[buffer_tail];
    buffer_tail = (buffer_tail + 1) % KEYBOARD_BUFFER_SIZE;
    return c;
}

uint8_t keyboard_get_scancode(void) {
    return keyboard_read_data();
}