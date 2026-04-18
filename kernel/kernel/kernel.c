#include <stdio.h>
#include <string.h>
#include <stdint.h>

#include <kernel/gdt.h>
#include <kernel/tty.h>
#include <kernel/idt.h>
#include <kernel/apic.h>
#include <kernel/pmm.h>
#include <kernel/vmm.h>
#include <kernel/keyboard.h>
#include <kernel/timer.h>
#include <kernel/scheduler.h>
#include <kernel/disk.h>
#include <kernel/fat32.h>

// Simple command buffer
#define CMD_BUFFER_SIZE 256
static char cmd_buffer[CMD_BUFFER_SIZE];
static int cmd_pos = 0;

// Current directory
static char current_dir[256] = "/";

// Simple shell functions
static void shell_prompt(void) {
    printf("%s> ", current_dir);
}

static void shell_help(void) {
    printf("Available commands:\n");
    printf("  help     - Show this help message\n");
    printf("  clear    - Clear the screen\n");
    printf("  mem      - Show memory information\n");
    printf("  tasks    - Show running tasks\n");
    printf("  reboot   - Reboot the system\n");
    printf("  echo     - Echo text back\n");
    printf("  uname    - Show system information\n");
    printf("  time     - Show system ticks\n");
    printf("  disk     - Show disk information\n");
    printf("  mount    - Mount FAT32 filesystem\n");
    printf("  ls       - List directory contents\n");
    printf("  cat      - Display file contents\n");
    printf("  touch    - Create a new file\n");
    printf("  mkdir    - Create a new directory\n");
    printf("  write    - Write text to a file\n");
}

static void shell_clear(void) {
    terminal_initialize();
}

static void shell_mem(void) {
    size_t total = pmm_get_total_memory();
    size_t free = pmm_get_free_memory();
    size_t used = total - free;
    
    printf("Memory Information:\n");
    printf("  Total: %u KB (%u MB)\n", (uint32_t)(total / 1024), (uint32_t)(total / 1024 / 1024));
    printf("  Used:  %u KB\n", (uint32_t)(used / 1024));
    printf("  Free:  %u KB\n", (uint32_t)(free / 1024));
}

static void shell_tasks(void) {
    printf("Running tasks: %u\n", scheduler_get_task_count());
    printf("Current task: %s (PID: %u)\n", 
           scheduler_get_current()->name,
           scheduler_get_current()->pid);
}

static void shell_reboot(void) {
    printf("Rebooting...\n");
    // Try keyboard controller reset
    uint8_t good = 0x02;
    while (good & 0x02) {
        __asm__ __volatile__ ("inb %%dx, %%al" : "=a"(good) : "d"(0x64));
    }
    __asm__ __volatile__ ("outb %%al, %%dx" : : "a"(0xFE), "d"(0x64));
    // If that fails, triple fault
    __asm__ __volatile__ ("int $0xFF");
}

static void shell_echo(const char* text) {
    printf("%s\n", text);
}

static void shell_uname(void) {
    printf("MyOS v1.0\n");
    printf("Kernel: Custom i386 Kernel\n");
    printf("Features: GDT, IDT, APIC, Paging, Scheduler, FAT32 Filesystem\n");
}

static void shell_time(void) {
    printf("System ticks: %u\n", timer_get_ticks());
    printf("Uptime: ~%u seconds\n", timer_get_ticks() / 100);
}

static void shell_disk(void) {
    printf("Initializing disk subsystem...\n");
    if (!disk_init()) {
        printf("No disks detected.\n");
        return;
    }
    
    uint32_t count = disk_get_count();
    printf("Detected %u disk(s):\n", count);
    
    for (uint32_t i = 0; i < count; i++) {
        disk_print_info(i);
    }
}

static void shell_mount(void) {
    if (!fat32_is_mounted()) {
        printf("Mounting FAT32 filesystem...\n");
        if (fat32_mount(0)) {
            printf("Filesystem mounted successfully.\n");
        } else {
            printf("Failed to mount filesystem.\n");
        }
    } else {
        printf("Filesystem already mounted.\n");
    }
}

static void shell_ls(void) {
    if (!fat32_is_mounted()) {
        printf("No filesystem mounted. Use 'mount' first.\n");
        return;
    }
    
    fat32_dir_t dir;
    fat32_direntry_t entry;
    char name[256];
    
    if (!fat32_opendir(&dir, current_dir)) {
        printf("Failed to open directory.\n");
        return;
    }
    
    printf("Contents of %s:\n", current_dir);
    printf("%-12s %10s %s\n", "Name", "Size", "Attr");
    printf("---------------------------------------\n");
    
    while (fat32_readdir(&dir, &entry, name)) {
        char attr_str[5] = "----";
        if (entry.attributes & FAT_ATTR_DIRECTORY) attr_str[0] = 'D';
        if (entry.attributes & FAT_ATTR_READ_ONLY) attr_str[1] = 'R';
        if (entry.attributes & FAT_ATTR_HIDDEN) attr_str[2] = 'H';
        if (entry.attributes & FAT_ATTR_SYSTEM) attr_str[3] = 'S';
        
        printf("%-12s %10u %s\n", name, entry.file_size, attr_str);
    }
    
    fat32_closedir(&dir);
}

static void shell_cat(const char* filename) {
    if (!fat32_is_mounted()) {
        printf("No filesystem mounted. Use 'mount' first.\n");
        return;
    }
    
    if (!filename || strlen(filename) == 0) {
        printf("Usage: cat <filename>\n");
        return;
    }
    
    fat32_file_t file;
    if (!fat32_open(&file, filename)) {
        printf("File not found: %s\n", filename);
        return;
    }
    
    if (file.is_directory) {
        printf("%s is a directory.\n", filename);
        fat32_close(&file);
        return;
    }
    
    // Read and display file
    char buffer[513];
    size_t bytes_read;
    
    while ((bytes_read = fat32_read(&file, buffer, 512)) > 0) {
        buffer[bytes_read] = '\0';
        printf("%s", buffer);
    }
    printf("\n");
    
    fat32_close(&file);
}

static void shell_touch(const char* filename) {
    if (!fat32_is_mounted()) {
        printf("No filesystem mounted. Use 'mount' first.\n");
        return;
    }
    
    if (!filename || strlen(filename) == 0) {
        printf("Usage: touch <filename>\n");
        return;
    }
    
    if (fat32_create(filename, FAT_ATTR_ARCHIVE)) {
        printf("Created file: %s\n", filename);
    } else {
        printf("Failed to create file: %s\n", filename);
    }
}

static void shell_mkdir(const char* dirname) {
    if (!fat32_is_mounted()) {
        printf("No filesystem mounted. Use 'mount' first.\n");
        return;
    }
    
    if (!dirname || strlen(dirname) == 0) {
        printf("Usage: mkdir <dirname>\n");
        return;
    }
    
    if (fat32_mkdir(dirname)) {
        printf("Created directory: %s\n", dirname);
    } else {
        printf("Failed to create directory: %s\n", dirname);
    }
}

static void shell_write(const char* args) {
    if (!fat32_is_mounted()) {
        printf("No filesystem mounted. Use 'mount' first.\n");
        return;
    }
    
    if (!args || strlen(args) == 0) {
        printf("Usage: write <filename> <text>\n");
        return;
    }
    
    // Find first space to separate filename and text
    char* text = strchr(args, ' ');
    if (!text) {
        printf("Usage: write <filename> <text>\n");
        return;
    }
    
    // Extract filename
    char filename[256];
    int len = text - args;
    if (len >= 256) len = 255;
    strncpy(filename, args, len);
    filename[len] = '\0';
    
    text++;  // Skip space
    
    // Open file (create if doesn't exist)
    fat32_file_t file;
    if (!fat32_open(&file, filename)) {
        // Try to create it
        if (!fat32_create(filename, FAT_ATTR_ARCHIVE)) {
            printf("Failed to create file: %s\n", filename);
            return;
        }
        if (!fat32_open(&file, filename)) {
            printf("Failed to open file: %s\n", filename);
            return;
        }
    }
    
    if (file.is_directory) {
        printf("%s is a directory.\n", filename);
        fat32_close(&file);
        return;
    }
    
    // Seek to end of file (append mode)
    fat32_seek(&file, file.size);
    
    // Write text
    size_t written = fat32_write(&file, text, strlen(text));
    fat32_write(&file, "\n", 1);  // Add newline
    
    printf("Wrote %u bytes to %s\n", (uint32_t)written + 1, filename);
    
    fat32_close(&file);
}

static void shell_execute(void) {
    printf("\n");
    
    // Parse command
    char* cmd = cmd_buffer;
    while (*cmd == ' ') cmd++; // Skip leading spaces
    
    if (strlen(cmd) == 0) {
        // Empty command
    } else if (strcmp(cmd, "help") == 0) {
        shell_help();
    } else if (strcmp(cmd, "clear") == 0) {
        shell_clear();
    } else if (strcmp(cmd, "mem") == 0) {
        shell_mem();
    } else if (strcmp(cmd, "tasks") == 0) {
        shell_tasks();
    } else if (strcmp(cmd, "reboot") == 0) {
        shell_reboot();
    } else if (strcmp(cmd, "uname") == 0) {
        shell_uname();
    } else if (strcmp(cmd, "time") == 0) {
        shell_time();
    } else if (strcmp(cmd, "disk") == 0) {
        shell_disk();
    } else if (strcmp(cmd, "mount") == 0) {
        shell_mount();
    } else if (strcmp(cmd, "ls") == 0) {
        shell_ls();
    } else if (strncmp(cmd, "cat ", 4) == 0) {
        shell_cat(cmd + 4);
    } else if (strncmp(cmd, "touch ", 6) == 0) {
        shell_touch(cmd + 6);
    } else if (strncmp(cmd, "mkdir ", 6) == 0) {
        shell_mkdir(cmd + 6);
    } else if (strncmp(cmd, "write ", 6) == 0) {
        shell_write(cmd + 6);
    } else if (strncmp(cmd, "echo ", 5) == 0) {
        shell_echo(cmd + 5);
    } else if (strncmp(cmd, "echo", 4) == 0) {
        shell_echo("");
    } else {
        printf("Unknown command: %s\n", cmd);
        printf("Type 'help' for available commands.\n");
    }
    
    cmd_pos = 0;
    cmd_buffer[0] = '\0';
    shell_prompt();
}

static void shell_handle_char(char c) {
    if (c == '\n' || c == '\r') {
        cmd_buffer[cmd_pos] = '\0';
        shell_execute();
    } else if (c == '\b') {
        if (cmd_pos > 0) {
            cmd_pos--;
            cmd_buffer[cmd_pos] = '\0';
            // Use putchar for backspace to ensure it works
            putchar('\b');
            putchar(' ');
            putchar('\b');
        }
    } else if (c >= 32 && c < 127 && cmd_pos < CMD_BUFFER_SIZE - 1) {
        cmd_buffer[cmd_pos++] = c;
        cmd_buffer[cmd_pos] = '\0';
        putchar(c);
    }
}

// Demo task
static void demo_task(void) {
    uint32_t count = 0;
    while (1) {
        // This task would run concurrently
        count++;
        if (count % 10000000 == 0) {
            // Would print but let's not clutter the shell
        }
        task_yield();
    }
}

void kernel_main(void)
{
    // Initialize terminal first
    terminal_initialize();
    
    // Welcome message
    printf("\n");
    printf("========================================\n");
    printf("       Welcome to MyOS v1.0!           \n");
    printf("========================================\n");
    printf("\n");
    
    // Initialize core systems
    printf("Initializing GDT... ");
    gdt_init();
    printf("OK\n");
    
    printf("Initializing IDT... ");
    idt_init();
    printf("OK\n");
    
    printf("Initializing Physical Memory Manager... ");
    // Initialize with 128MB of RAM
    pmm_init_region(0x100000, 0x8000000); // 1MB to 128MB+1MB
    printf("OK\n");
    
    printf("Initializing Virtual Memory Manager... ");
    vmm_init();
    printf("OK\n");
    
    printf("Initializing APIC... ");
    enable_apic();
    printf("OK\n");
    
    printf("Initializing Timer (100Hz)... ");
    timer_init(100);
    printf("OK\n");
    
    printf("Initializing Keyboard... ");
    keyboard_init();
    printf("OK\n");
    
    printf("Initializing Scheduler... ");
    scheduler_init();
    printf("OK\n");
    
    printf("Initializing FAT32... ");
    fat32_init();
    printf("OK\n");
    
    // Create a demo task
    task_create("demo", demo_task);
    
    printf("\n");
    printf("System ready!\n");
    printf("Type 'help' for available commands.\n");
    printf("\n");
    
    // Start shell
    shell_prompt();
    
    // Main loop - read keyboard input
    while (1) {
        if (keyboard_has_key()) {
            char c = keyboard_getchar();
            shell_handle_char(c);
        }
        
        // Yield to let other tasks run
        __asm__ __volatile__ ("sti; hlt");
    }
}