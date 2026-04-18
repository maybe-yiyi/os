#include <stdint.h>
#include <stddef.h>
#include <string.h>
#include <kernel/scheduler.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/timer.h>
#include <kernel/vmm.h>
#include <kernel/pmm.h>
#include <kernel/timer.h>

// Task list
static task_t* current_task = NULL;
static task_t* task_list = NULL;
static uint32_t next_pid = 1;
static uint32_t task_count = 0;

// Idle task stack
static uint8_t idle_stack[STACK_SIZE] __attribute__((aligned(16)));

// Assembly context switch
extern void context_switch(task_context_t* prev, task_context_t* next);

// Get ESP0 from TSS (for ring 0)
static inline uint32_t read_esp(void) {
    uint32_t esp;
    __asm__ __volatile__ ("mov %%esp, %0" : "=r"(esp));
    return esp;
}

static void idle_task(void) {
    while (1) {
        __asm__ __volatile__ ("sti; hlt");
    }
}

void scheduler_init(void) {
    // Initialize task list
    task_list = NULL;
    current_task = NULL;
    task_count = 0;
    next_pid = 1;
    
    // Create idle task
    task_t* idle = task_create("idle", idle_task);
    if (idle) {
        idle->state = TASK_READY;
    }
}

task_t* task_create(const char* name, void (*entry)(void)) {
    if (task_count >= MAX_TASKS) {
        return NULL;
    }
    
    // Allocate task structure
    task_t* task = (task_t*)vmm_alloc_page(PAGE_WRITABLE);
    if (!task) {
        return NULL;
    }
    
    // Allocate stack
    uint8_t* stack = (uint8_t*)vmm_alloc_page(PAGE_WRITABLE);
    if (!stack) {
        vmm_free_page(task);
        return NULL;
    }
    
    // Initialize task
    memset(task, 0, sizeof(task_t));
    task->pid = next_pid++;
    strncpy(task->name, name, 31);
    task->name[31] = '\0';
    task->state = TASK_READY;
    task->stack = stack;
    task->stack_size = PAGE_SIZE;
    
    // Set up initial context
    task->context.eip = (uint32_t)entry;
    task->context.esp = (uint32_t)(stack + PAGE_SIZE - 16); // Stack grows down
    task->context.eflags = 0x200; // Interrupts enabled
    task->context.cr3 = (uint32_t)vmm_get_current_directory();
    
    // Add to task list
    task->next = task_list;
    task_list = task;
    task_count++;
    
    return task;
}

void schedule(void) {
    if (!current_task) {
        // First schedule
        current_task = task_list;
        return;
    }
    
    // Find next ready task
    task_t* next = current_task->next;
    if (!next) {
        next = task_list;
    }
    
    // Look for ready task
    task_t* start = next;
    do {
        if (next->state == TASK_READY) {
            break;
        }
        if (next->state == TASK_SLEEPING) {
            if (timer_get_ticks() >= next->sleep_until) {
                next->state = TASK_READY;
                break;
            }
        }
        next = next->next;
        if (!next) {
            next = task_list;
        }
    } while (next != start);
    
    // If no ready task found, use idle
    if (next->state != TASK_READY) {
        // Find idle task
        next = task_list;
        while (next && strcmp(next->name, "idle") != 0) {
            next = next->next;
        }
        if (!next) {
            next = task_list; // Fallback
        }
    }
    
    if (next != current_task) {
        task_t* prev = current_task;
        current_task = next;
        
        // Switch page directory if different
        if (prev->context.cr3 != next->context.cr3) {
            __asm__ __volatile__ ("mov %0, %%cr3" : : "r"(next->context.cr3));
        }
        
        // Context switch
        context_switch(&prev->context, &next->context);
    }
}

task_t* scheduler_get_current(void) {
    return current_task;
}

void task_block(void) {
    if (current_task) {
        current_task->state = TASK_BLOCKED;
    }
    task_yield();
}

void task_unblock(task_t* task) {
    if (task) {
        task->state = TASK_READY;
    }
}

void task_sleep(uint32_t ms) {
    if (current_task) {
        current_task->sleep_until = timer_get_ticks() + (ms * 100) / 1000; // Approximate
        current_task->state = TASK_SLEEPING;
    }
    task_yield();
}

void task_yield(void) {
    // Trigger timer interrupt for reschedule
    __asm__ __volatile__ ("int $0x20");
}

uint32_t scheduler_get_task_count(void) {
    return task_count;
}