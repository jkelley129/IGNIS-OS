#include "task.h"
#include "mm/allocators/kmalloc.h"
#include "libc/string.h"
#include "console/console.h"
#include "io/serial.h"

static task_t* task_table[MAX_TASKS];
static uint32_t next_pid = 0;
static task_t* current_task = NULL;
static task_t* idle_task = NULL;

// Simple round-robin queue
static task_t* ready_queue_head = NULL;
static task_t* ready_queue_tail = NULL;

#define TIME_SLICE_TICKS 10  // 100ms at 100Hz

// Task exit function - called when task returns
static void task_exit(void) {
    serial_debug_puts("[TASK] Task ");
    serial_debug_puts(current_task->name);
    serial_debug_puts(" exited\n");

    if (current_task) {
        current_task->state = TERMINATED;
        scheduler_remove_task(current_task);
    }

    // Force immediate context switch
    while(1) {
        task_yield();
    }
}

// Idle task - runs when nothing else can
static void idle_task_entry(void) {
    while(1) {
        asm volatile("hlt");
    }
}

kerr_t task_init(void) {
    // Clear task table
    for (int i = 0; i < MAX_TASKS; i++) {
        task_table[i] = NULL;
    }

    current_task = NULL;
    ready_queue_head = NULL;
    ready_queue_tail = NULL;

    serial_debug_puts("[TASK] Task system initialized\n");
    return E_OK;
}

kerr_t scheduler_init(void) {
    // Create idle task
    idle_task = task_create("idle", idle_task_entry);
    if (!idle_task) {
        serial_debug_puts("[SCHEDULER] Failed to create idle task!\n");
        return E_NOMEM;
    }

    idle_task->state = RUNNING;
    current_task = idle_task;

    serial_debug_puts("[SCHEDULER] Scheduler initialized with idle task (PID ");
    char pid_str[16];
    uitoa(idle_task->pid, pid_str);
    serial_debug_puts(pid_str);
    serial_debug_puts(")\n");

    return E_OK;
}

task_t* task_create(const char* name, void (*entry_point)(void)) {
    // Find free slot
    uint32_t pid = next_pid;
    if (pid >= MAX_TASKS) {
        serial_debug_puts("[TASK] Task table full!\n");
        return NULL;
    }

    // Allocate task structure
    task_t* task = kmalloc(sizeof(task_t));
    if (!task) {
        return NULL;
    }

    // Allocate stack
    task->stack_base = kmalloc(TASK_STACK_SIZE);
    if (!task->stack_base) {
        kfree(task);
        return NULL;
    }

    // Stack grows down, so top is at base + size
    task->stack_top = task->stack_base + TASK_STACK_SIZE;

    // Initialize task structure
    task->pid = pid;
    strncpy(task->name, name, 31);
    task->name[31] = '\0';
    task->state = READY;
    task->time_slice = TIME_SLICE_TICKS;
    task->total_runtime = 0;
    task->next = NULL;

    // Setup initial stack with context
    uint64_t* stack_ptr = (uint64_t*)task->stack_top;

    // SS (stack segment)
    stack_ptr--;
    *stack_ptr = 0x10;  // Kernel data segment

    // RSP - stack pointer (points to just below this structure)
    stack_ptr--;
    *stack_ptr = (uint64_t)stack_ptr;  // Will be updated during switch

    // RFLAGS - CPU flags
    stack_ptr--;
    *stack_ptr = 0x202;  // Interrupts enabled (IF=1)

    // CS (code segment)
    stack_ptr--;
    *stack_ptr = 0x08;  // Kernel code segment

    // RIP - instruction pointer (where to start)
    stack_ptr--;
    *stack_ptr = (uint64_t)entry_point;

    // General purpose registers (all zero initially)
    stack_ptr--; *stack_ptr = 0;  // RAX
    stack_ptr--; *stack_ptr = 0;  // RBX
    stack_ptr--; *stack_ptr = 0;  // RCX
    stack_ptr--; *stack_ptr = 0;  // RDX
    stack_ptr--; *stack_ptr = 0;  // RSI
    stack_ptr--; *stack_ptr = 0;  // RDI
    stack_ptr--; *stack_ptr = 0;  // RBP
    stack_ptr--; *stack_ptr = 0;  // R8
    stack_ptr--; *stack_ptr = 0;  // R9
    stack_ptr--; *stack_ptr = 0;  // R10
    stack_ptr--; *stack_ptr = 0;  // R11
    stack_ptr--; *stack_ptr = 0;  // R12
    stack_ptr--; *stack_ptr = 0;  // R13
    stack_ptr--; *stack_ptr = 0;  // R14
    stack_ptr--; *stack_ptr = 0;  // R15

    // Context points to this prepared stack
    task->context = (cpu_state_t*)stack_ptr;

    // Add to task table
    task_table[pid] = task;
    next_pid++;

    serial_debug_puts("[TASK] Created task: ");
    serial_debug_puts(name);
    serial_debug_puts(" (PID ");
    char pid_str[16];
    uitoa(pid, pid_str);
    serial_debug_puts(pid_str);
    serial_debug_puts(")\n");

    return task;
}

void task_destroy(task_t* task) {
    if (!task) return;

    serial_debug_puts("[TASK] Destroying task: ");
    serial_debug_puts(task->name);
    serial_debug_puts("\n");

    // Remove from scheduler
    scheduler_remove_task(task);

    // Free resources
    if (task->stack_base) {
        kfree(task->stack_base);
    }

    // Remove from table
    if (task->pid < MAX_TASKS) {
        task_table[task->pid] = NULL;
    }

    kfree(task);
}

task_t* task_get_current(void) {
    return current_task;
}

void scheduler_add_task(task_t* task) {
    if (!task) return;

    task->state = READY;
    task->next = NULL;

    if (!ready_queue_head) {
        ready_queue_head = task;
        ready_queue_tail = task;
    } else {
        ready_queue_tail->next = task;
        ready_queue_tail = task;
    }

    serial_debug_puts("[SCHEDULER] Added task to ready queue: ");
    serial_debug_puts(task->name);
    serial_debug_puts("\n");
}

void scheduler_remove_task(task_t* task) {
    if (!task || !ready_queue_head) return;

    // Special case: removing head
    if (ready_queue_head == task) {
        ready_queue_head = task->next;
        if (!ready_queue_head) {
            ready_queue_tail = NULL;
        }
        task->next = NULL;
        return;
    }

    // Search and remove
    task_t* prev = ready_queue_head;
    task_t* curr = ready_queue_head->next;

    while (curr) {
        if (curr == task) {
            prev->next = curr->next;
            if (ready_queue_tail == curr) {
                ready_queue_tail = prev;
            }
            task->next = NULL;

            serial_debug_puts("[SCHEDULER] Removed task from ready queue: ");
            serial_debug_puts(task->name);
            serial_debug_puts("\n");

            return;
        }
        prev = curr;
        curr = curr->next;
    }
}

task_t* scheduler_pick_next(void) {
    // Round-robin: pick head of queue
    if (!ready_queue_head) {
        return idle_task;  // No tasks ready, run idle
    }

    task_t* next = ready_queue_head;

    // Remove from head
    ready_queue_head = next->next;
    if (!ready_queue_head) {
        ready_queue_tail = NULL;
    }
    next->next = NULL;

    return next;
}

void scheduler_tick(void) {
    if (!current_task) return;

    // Decrement time slice
    if (current_task->time_slice > 0) {
        current_task->time_slice--;
    }

    // Increment runtime
    current_task->total_runtime++;

    // Time slice expired? Switch tasks
    if (current_task->time_slice == 0) {
        task_t* old_task = current_task;
        task_t* new_task = scheduler_pick_next();

        if (new_task && new_task != old_task) {
            // Put old task back in queue if still ready
            if (old_task->state == RUNNING) {
                old_task->state = READY;
                old_task->time_slice = TIME_SLICE_TICKS;
                scheduler_add_task(old_task);
            }

            // Switch to new task
            new_task->state = RUNNING;
            new_task->time_slice = TIME_SLICE_TICKS;
            current_task = new_task;

            // Debug: Log context switch (but not too frequently)
            static uint64_t switch_count = 0;
            switch_count++;
            if (switch_count % 100 == 0) {
                serial_debug_puts("[SCHEDULER] Context switch: ");
                serial_debug_puts(old_task->name);
                serial_debug_puts(" -> ");
                serial_debug_puts(new_task->name);
                serial_debug_puts("\n");
            }

            // Perform context switch
            task_switch(&old_task->context, new_task->context);
        } else {
            // Just reset time slice
            current_task->time_slice = TIME_SLICE_TICKS;
        }
    }
}

void task_yield(void) {
    if (!current_task) return;

    current_task->time_slice = 0;  // Force switch
    scheduler_tick();
}

void task_block(void) {
    if (!current_task) return;

    serial_debug_puts("[TASK] Blocking task: ");
    serial_debug_puts(current_task->name);
    serial_debug_puts("\n");

    current_task->state = BLOCKED;
    task_yield();
}

void task_unblock(task_t* task) {
    if (!task || task->state != BLOCKED) return;

    serial_debug_puts("[TASK] Unblocking task: ");
    serial_debug_puts(task->name);
    serial_debug_puts("\n");

    scheduler_add_task(task);
}

// Utility function to print task list (for debugging)
void task_print_list(void) {
    console_puts("\n=== Task List ===\n");
    console_puts("PID  Name            State      Runtime\n");
    console_puts("-------------------------------------------\n");

    for (uint32_t i = 0; i < next_pid; i++) {
        if (task_table[i]) {
            task_t* t = task_table[i];
            char num_str[32];

            // PID
            uitoa(t->pid, num_str);
            console_puts(num_str);

            // Padding
            size_t len = strlen(num_str);
            for (size_t j = len; j < 5; j++) console_putc(' ');

            // Name
            console_puts(t->name);
            len = strlen(t->name);
            for (size_t j = len; j < 16; j++) console_putc(' ');

            // State
            const char* state_str;
            console_color_attr_t state_color;
            switch (t->state) {
                case READY:
                    state_str = "READY";
                    state_color = (console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK};
                    break;
                case RUNNING:
                    state_str = "RUNNING";
                    state_color = (console_color_attr_t){CONSOLE_COLOR_LIGHT_GREEN, CONSOLE_COLOR_BLACK};
                    break;
                case BLOCKED:
                    state_str = "BLOCKED";
                    state_color = (console_color_attr_t){CONSOLE_COLOR_BROWN, CONSOLE_COLOR_BLACK};
                    break;
                case TERMINATED:
                    state_str = "TERMINATED";
                    state_color = (console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK};
                    break;
                default:
                    state_str = "UNKNOWN";
                    state_color = (console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK};
            }

            console_puts_color(state_str, state_color);
            len = strlen(state_str);
            for (size_t j = len; j < 11; j++) console_putc(' ');

            // Runtime
            uitoa(t->total_runtime, num_str);
            console_puts(num_str);
            console_puts(" ticks\n");
        }
    }

    console_puts("\nCurrent task: ");
    if (current_task) {
        console_puts(current_task->name);
    } else {
        console_puts("(none)");
    }
    console_puts("\n\n");
}