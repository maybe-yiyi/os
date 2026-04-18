#ifndef _KERNEL_SCHEDULER_H
#define _KERNEL_SCHEDULER_H

#include <stdint.h>
#include <stdbool.h>

#define MAX_TASKS 256
#define STACK_SIZE 4096
#define KERNEL_CS 0x08
#define KERNEL_DS 0x10
#define USER_CS 0x1B
#define USER_DS 0x23

typedef enum {
    TASK_RUNNING,
    TASK_READY,
    TASK_BLOCKED,
    TASK_SLEEPING,
    TASK_TERMINATED
} task_state_t;

typedef struct {
    uint32_t eax, ebx, ecx, edx;
    uint32_t esi, edi, ebp;
    uint32_t esp, eip;
    uint32_t eflags;
    uint32_t cr3;
} task_context_t;

typedef struct task {
    uint32_t pid;
    char name[32];
    task_state_t state;
    task_context_t context;
    uint8_t* stack;
    uint32_t stack_size;
    uint32_t sleep_until;
    struct task* next;
} task_t;

// Initialize scheduler
void scheduler_init(void);

// Create a new kernel task
task_t* task_create(const char* name, void (*entry)(void));

// Create user task
task_t* task_create_user(const char* name, void (*entry)(void));

// Schedule next task
void schedule(void);

// Get current task
task_t* scheduler_get_current(void);

// Block current task
void task_block(void);

// Unblock a task
void task_unblock(task_t* task);

// Sleep current task
void task_sleep(uint32_t ms);

// Yield CPU
void task_yield(void);

// Get number of tasks
uint32_t scheduler_get_task_count(void);

#endif