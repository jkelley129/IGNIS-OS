#ifndef TASK_H
#define TASK_H

#include "libc/stdint.h"
#include "error_handling/errno.h"

#define MAX_TASKS 64
#define TASK_STACK_SIZE 8192  // 8KB stack per task

typedef enum {
    READY,
    RUNNING,
    BLOCKED,
    SLEEPING,
    TERMINATED,
} task_state_t;

// CPU register state for context switching
typedef struct {
    // Callee-saved registers (System V AMD64 ABI)
    uint64_t rbx;
    uint64_t rbp;
    uint64_t r12;
    uint64_t r13;
    uint64_t r14;
    uint64_t r15;
    uint64_t rip;  // Return address
} __attribute__((packed)) cpu_state_t;

typedef struct task {
    uint32_t pid;                    // Task ID
    char name[32];                   // Task name for debugging
    task_state_t state;              // Current state
    cpu_state_t* context;            // Saved CPU state
    void* stack_base;                // Bottom of stack
    void* stack_top;                 // Top of stack (grows down)
    uint64_t time_slice;             // Remaining time slice (in ticks)
    uint64_t total_runtime;          // Total ticks this task has run
    uint64_t wake_time;              // Tick count to wake up for
    struct task* next;               // Next task in scheduler queue
} task_t;

// Task management API
kerr_t task_init(void);
task_t* task_create(const char* name, void (*entry_point)(void));
void task_exit(void);
void task_destroy(task_t* task);
task_t* task_get_current(void);
void task_yield(void);
void task_block(void);
void task_unblock(task_t* task);
void task_sleep(uint64_t ticks);
void task_print_list(void);

// Scheduler functions
kerr_t scheduler_init(void);
void scheduler_add_task(task_t* task);
void scheduler_remove_task(task_t* task);
task_t* scheduler_pick_next(void);
void scheduler_tick(void);  // Called from PIT handler

// Context switching (implemented in assembly)
void task_switch(cpu_state_t** old_context, cpu_state_t* new_context);

#endif