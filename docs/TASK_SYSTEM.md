# IGNIS OS - Task and Scheduler System

## Overview

IGNIS implements a preemptive multitasking system with a simple round-robin scheduler. Tasks can be created, scheduled, and switched between automatically by the timer interrupt (PIT). This document covers the task management system, context switching mechanism, and scheduler implementation.

## Table of Contents

- [Architecture](#architecture)
- [Task Structure](#task-structure)
- [Task States](#task-states)
- [Context Switching](#context-switching)
- [Scheduler Design](#scheduler-design)
- [Ready Queue Mechanics](#ready-queue-mechanics)
- [Time Slicing](#time-slicing)
- [API Reference](#api-reference)
- [Usage Examples](#usage-examples)
- [Debugging Tasks](#debugging-tasks)

## Architecture

```
┌─────────────────────────────────────────────────────────┐
│                     Application                         │
│              (Shell, Future User Programs)              │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                   Task Management API                    │
│   task_create() | task_yield() | task_block()          │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                 Round-Robin Scheduler                    │
│   scheduler_add_task() | scheduler_pick_next()         │
│   scheduler_tick() | scheduler_remove_task()           │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                  Context Switch (ASM)                    │
│   task_switch(old_context, new_context)                │
│   - Save callee-saved registers                         │
│   - Switch stacks                                        │
│   - Restore registers                                    │
└─────────────────────────────────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────────┐
│                    PIT Timer (IRQ0)                      │
│   Fires every 10ms (100 Hz)                            │
│   Calls scheduler_tick() → context switch              │
└─────────────────────────────────────────────────────────┘
```

## Task Structure

### Task Control Block (TCB)

Every task in IGNIS is represented by a `task_t` structure:

```c
typedef struct task {
    uint32_t pid;                    // Unique process ID
    char name[32];                   // Human-readable name (for debugging)
    task_state_t state;              // Current state (READY/RUNNING/BLOCKED/TERMINATED)
    cpu_state_t* context;            // Saved CPU state (register values)
    void* stack_base;                // Bottom of stack memory
    void* stack_top;                 // Top of stack (grows downward)
    uint64_t time_slice;             // Remaining ticks before preemption
    uint64_t total_runtime;          // Total CPU time used (in ticks)
    struct task* next;               // Pointer to next task in queue
} task_t;
```

**Field Descriptions:**

| Field | Purpose |
|-------|---------|
| `pid` | Unique identifier assigned sequentially (0, 1, 2, ...) |
| `name` | Descriptive name like "shell", "idle", "network" |
| `state` | Current execution state (see Task States below) |
| `context` | Pointer to saved registers on this task's stack |
| `stack_base` | Lowest address of 8KB stack allocation |
| `stack_top` | Highest address (stack grows down from here) |
| `time_slice` | Countdown timer (10 ticks = 100ms at 100Hz) |
| `total_runtime` | Performance metric for debugging |
| `next` | Forms linked list for scheduler queue |

### CPU Context Structure

The `cpu_state_t` contains the minimal register set needed for context switching:

```c
typedef struct {
    uint64_t rbx;    // Callee-saved general purpose register
    uint64_t rbp;    // Base pointer (frame pointer)
    uint64_t r12;    // Callee-saved register
    uint64_t r13;    // Callee-saved register
    uint64_t r14;    // Callee-saved register
    uint64_t r15;    // Callee-saved register
    uint64_t rip;    // Return address (where to resume)
} __attribute__((packed)) cpu_state_t;
```

**Why only these registers?**

According to the System V AMD64 ABI (the calling convention used by Linux and most x86-64 systems):
- **Callee-saved registers** (RBX, RBP, R12-R15): Must be preserved across function calls
- **Caller-saved registers** (RAX, RCX, RDX, RSI, RDI, R8-R11): Can be clobbered by function calls
- Since task switching is essentially a "call", we only need to save callee-saved registers
- RIP (instruction pointer) is saved implicitly as the return address

## Task States

Tasks transition through different states during their lifetime:

```
    ┌──────────┐
    │  READY   │ ◄────────────────┐
    └──────────┘                  │
         │                        │
         │ scheduler_pick_next()  │
         ▼                        │
    ┌──────────┐                  │
    │ RUNNING  │                  │
    └──────────┘                  │
         │                        │
         ├─────────────────────┐  │
         │                     │  │
         │ time_slice = 0      │  │ task_unblock()
         │                     │  │
         ▼                     ▼  │
    ┌──────────┐          ┌──────────┐
    │  READY   │          │ BLOCKED  │
    └──────────┘          └──────────┘
         │                     
         │ task exits          
         ▼                     
    ┌──────────┐              
    │TERMINATED│              
    └──────────┘              
```

### State Descriptions

| State | Description | Can Run? |
|-------|-------------|----------|
| **READY** | Task is waiting in the scheduler queue | No |
| **RUNNING** | Task is currently executing on CPU | Yes |
| **BLOCKED** | Task is waiting for I/O or event | No |
| **TERMINATED** | Task has exited, awaiting cleanup | No |

**State Transitions:**

```c
// Task created → READY
task_t* task = task_create("myapp", entry_func);
scheduler_add_task(task);  // state = READY

// Scheduler picks task → RUNNING
// In scheduler_tick() when time slice expires:
task->state = RUNNING;

// Time slice expires → READY
// Task returns to queue for another turn:
old_task->state = READY;
scheduler_add_task(old_task);

// Task waits for I/O → BLOCKED
task_block();  // state = BLOCKED

// I/O completes → READY
task_unblock(task);  // state = READY, back in queue

// Task exits → TERMINATED
task->state = TERMINATED;
scheduler_remove_task(task);
```

## Context Switching

Context switching is the heart of multitasking - it saves the current task's state and restores another task's state.

### High-Level Overview

```
Current Task (Running)          New Task (Ready)
      │                              │
      │ 1. Save registers            │
      ├─────────────────────┐        │
      │                     │        │
      │ 2. Save stack ptr   │        │
      │    to old_context   │        │
      │                     │        │
      │ 3. Load new stack   │        │
      │    pointer          │        │
      │                     ▼        │
      │                 Switch!      │
      │                     │        │
      │ 4. Restore regs     │        │
      │                     │        │
      │ 5. Return           │        │
      └─────────────────────┼────────┘
                            ▼
                     New Task (Running)
```

### Assembly Implementation

The actual context switch happens in `scheduler/task.asm`:

```nasm
task_switch:
    ; === SAVE OLD CONTEXT ===
    ; Push callee-saved registers onto current stack
    push rbx
    push rbp
    push r12
    push r13
    push r14
    push r15
    
    ; Save stack pointer to old task's context
    ; *old_context = rsp
    mov [rdi], rsp
    
    ; === LOAD NEW CONTEXT ===
    ; Switch to new task's stack
    mov rsp, rsi
    
    ; Restore callee-saved registers from new stack
    pop r15
    pop r14
    pop r13
    pop r12
    pop rbp
    pop rbx
    
    ; Return to new task's saved RIP
    ret
```

### Stack Layout During Context Switch

**Before context switch (Old Task):**
```
Higher addresses
┌─────────────────┐
│   Task Data     │
├─────────────────┤
│   Local Vars    │
├─────────────────┤
│   Return Addr   │ ← Where task_switch was called from
├─────────────────┤
│      RBX        │ ← RSP after push rbx
├─────────────────┤
│      RBP        │
├─────────────────┤
│      R12        │
├─────────────────┤
│      R13        │
├─────────────────┤
│      R14        │
├─────────────────┤
│      R15        │ ← RSP saved to old_context
└─────────────────┘
Lower addresses
```

**After context switch (New Task):**
```
Higher addresses
┌─────────────────┐
│   Task Data     │
├─────────────────┤
│   Return Addr   │ ← Where this task was interrupted
├─────────────────┤
│      RBX        │ ← All registers restored
├─────────────────┤
│      RBP        │
├─────────────────┤
│      ...        │
└─────────────────┘ ← RSP now points here
Lower addresses

RET instruction pops return address → jumps to new task's code
```

### First-Time Task Execution

When a task is created, its stack is pre-initialized to look like it was already context-switched out:

```c
// In task_create():
uint64_t* stack_ptr = (uint64_t*)task->stack_top;

// Reserve space for alignment
stack_ptr -= 2;

// Push "return address" (actually the entry point)
stack_ptr--;
*stack_ptr = (uint64_t)entry_point;  // RIP

// Push zeroed callee-saved registers
stack_ptr--; *stack_ptr = 0;  // R15
stack_ptr--; *stack_ptr = 0;  // R14
stack_ptr--; *stack_ptr = 0;  // R13
stack_ptr--; *stack_ptr = 0;  // R12
stack_ptr--; *stack_ptr = 0;  // RBP
stack_ptr--; *stack_ptr = 0;  // RBX

// Context points to prepared stack
task->context = (cpu_state_t*)stack_ptr;
```

When this task is first switched to:
1. `task_switch` loads RSP from `task->context`
2. Pops all registers (getting zeros)
3. Executes `ret`
4. RET pops the entry point address
5. CPU jumps to entry point - **task begins executing!**

## Scheduler Design

IGNIS uses a **simple round-robin scheduler** with preemptive time slicing.

### Key Concepts

- **Round-Robin**: Each task gets an equal turn
- **Preemptive**: Tasks are forcibly interrupted after their time slice
- **Time Slice**: 10 timer ticks = 100ms at 100Hz PIT frequency
- **Fair Scheduling**: No task can monopolize the CPU

### Scheduler Data Structures

```c
// Global scheduler state
static task_t* current_task = NULL;      // Currently running task
static task_t* idle_task = NULL;         // Special task (runs when nothing else can)
static task_t* ready_queue_head = NULL;  // First task in queue
static task_t* ready_queue_tail = NULL;  // Last task in queue
```

### The Ready Queue

The ready queue is a **singly-linked list** of tasks waiting to run:

```
ready_queue_head                           ready_queue_tail
      │                                          │
      ▼                                          ▼
   ┌──────┐      ┌──────┐      ┌──────┐      ┌──────┐
   │Task A│─────▶│Task B│─────▶│Task C│─────▶│Task D│─────▶ NULL
   └──────┘      └──────┘      └──────┘      └──────┘
   PID: 1        PID: 2        PID: 3        PID: 4
   next ───┐     next ───┐     next ───┐     next = NULL
           │             │             │
           ▼             ▼             ▼
       (Task B)      (Task C)      (Task D)
```

**Important Properties:**
- Tasks are **dequeued from the head** (FIFO order)
- Tasks are **enqueued at the tail**
- Each task's `next` pointer links to the following task
- Last task's `next` pointer is NULL
- Empty queue: both head and tail are NULL

## Ready Queue Mechanics

Let's walk through the exact operations with detailed examples.

### Initial State: Empty Queue

```
ready_queue_head = NULL
ready_queue_tail = NULL
current_task = idle_task (PID 0, state = RUNNING)

Queue visualization:
(empty)
```

### Adding First Task: Shell (PID 1)

```c
scheduler_add_task(shell_task);
```

**Code execution:**
```c
void scheduler_add_task(task_t* task) {
    task->state = READY;
    task->next = NULL;
    
    if (!ready_queue_head) {  // Queue is empty!
        ready_queue_head = task;
        ready_queue_tail = task;
    }
}
```

**Result:**
```
ready_queue_head ───┐
ready_queue_tail ───┼───▶ [Shell Task PID:1, next=NULL]
                    │
                    └───────┘
```

### Timer Tick #1: Scheduler Runs

```c
void scheduler_tick(void) {
    current_task->time_slice--;  // Idle: 9 ticks left
    
    if (current_task->time_slice == 0) {
        // Time expired! (not yet in this example)
    }
}
```

After 10 ticks, idle task's time slice expires...

### Timer Tick #10: Context Switch Occurs

```c
void scheduler_tick(void) {
    current_task->time_slice--;  // Idle: 0 ticks left
    current_task->total_runtime++;
    
    if (current_task->time_slice == 0) {
        task_t* old_task = current_task;        // old = Idle
        task_t* new_task = scheduler_pick_next(); // new = Shell
        
        // Put old task back in queue if still RUNNING
        if (old_task->state == RUNNING) {
            old_task->state = READY;
            old_task->time_slice = 10;  // Reset time slice
            scheduler_add_task(old_task);
        }
        
        // Activate new task
        new_task->state = RUNNING;
        new_task->time_slice = 10;
        current_task = new_task;
        
        // Perform actual context switch
        task_switch(&old_task->context, new_task->context);
    }
}
```

**Inside scheduler_pick_next():**
```c
task_t* scheduler_pick_next(void) {
    if (!ready_queue_head) {
        return idle_task;  // Nothing ready, run idle
    }
    
    // Dequeue head
    task_t* next = ready_queue_head;
    ready_queue_head = next->next;  // Advance head
    
    if (!ready_queue_head) {
        ready_queue_tail = NULL;  // Queue now empty
    }
    
    next->next = NULL;  // Unlink from queue
    return next;
}
```

**Queue state during this tick:**

**Before pick_next():**
```
ready_queue_head ────▶ [Shell PID:1, next=NULL]
ready_queue_tail ────▶ [Shell PID:1, next=NULL]

current_task ────▶ [Idle PID:0, state=RUNNING]
```

**After pick_next() (Shell removed from queue):**
```
ready_queue_head = NULL
ready_queue_tail = NULL

picked task = Shell PID:1
```

**After add_task(idle) (Idle added back to queue):**
```
ready_queue_head ────▶ [Idle PID:0, next=NULL]
ready_queue_tail ────▶ [Idle PID:0, next=NULL]

current_task ────▶ [Shell PID:1, state=RUNNING]
```

### Timer Tick #20: Another Context Switch

After Shell runs for 10 ticks:

**Before:**
```
Queue: [Idle PID:0, next=NULL]
Current: Shell PID:1 (time_slice=0)
```

**After scheduler_tick():**
```
Queue: [Shell PID:1, next=NULL]
Current: Idle PID:0 (time_slice=10)
```

### Adding Multiple Tasks

Let's say we create 3 more tasks:

```c
task_t* task_a = task_create("task_a", func_a);
task_t* task_b = task_create("task_b", func_b);
task_t* task_c = task_create("task_c", func_c);

scheduler_add_task(task_a);
scheduler_add_task(task_b);
scheduler_add_task(task_c);
```

**Queue after all additions:**
```
ready_queue_head                                    ready_queue_tail
      │                                                    │
      ▼                                                    ▼
   [Shell] ───▶ [TaskA] ───▶ [TaskB] ───▶ [TaskC] ───▶ NULL
   PID:1       PID:2        PID:3        PID:4
```

**Execution order over time:**
```
Tick 0-9:    Idle runs
Tick 10-19:  Shell runs     (Idle enqueued)
Tick 20-29:  Idle runs      (Shell enqueued)
Tick 30-39:  Shell runs     (Idle enqueued)
Tick 40-49:  TaskA runs     (Shell enqueued)
Tick 50-59:  TaskB runs     (TaskA enqueued)
Tick 60-69:  TaskC runs     (TaskB enqueued)
Tick 70-79:  Idle runs      (TaskC enqueued)
Tick 80-89:  Shell runs     (Idle enqueued)
... (cycle repeats)
```

### Queue Operations Detailed

#### Enqueue (Add to tail)

```c
void scheduler_add_task(task_t* task) {
    task->state = READY;
    task->next = NULL;
    
    if (!ready_queue_head) {
        // Empty queue: task becomes both head and tail
        ready_queue_head = task;
        ready_queue_tail = task;
    } else {
        // Non-empty queue: add to tail
        ready_queue_tail->next = task;
        ready_queue_tail = task;
    }
}
```

**Example: Adding Task E to [A→B→C]:**

Before:
```
Head ──▶ [A] ──▶ [B] ──▶ [C] ──▶ NULL
                          ▲
                         Tail
```

After `scheduler_add_task(E)`:
```
Head ──▶ [A] ──▶ [B] ──▶ [C] ──▶ [E] ──▶ NULL
                                  ▲
                                 Tail
```

#### Dequeue (Remove from head)

```c
task_t* scheduler_pick_next(void) {
    if (!ready_queue_head) {
        return idle_task;
    }
    
    task_t* next = ready_queue_head;
    ready_queue_head = next->next;
    
    if (!ready_queue_head) {
        ready_queue_tail = NULL;
    }
    
    next->next = NULL;
    return next;
}
```

**Example: Picking from [A→B→C→E]:**

Before:
```
Head ──▶ [A] ──▶ [B] ──▶ [C] ──▶ [E] ──▶ NULL
         ▲                        ▲
       Pick                      Tail
```

After:
```
Head ──▶ [B] ──▶ [C] ──▶ [E] ──▶ NULL
                          ▲
                         Tail

Returned: [A] (next=NULL, ready to run)
```

#### Remove (From middle/anywhere)

```c
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
            return;
        }
        prev = curr;
        curr = curr->next;
    }
}
```

**Example: Removing C from [A→B→C→E]:**

Before:
```
Head ──▶ [A] ──▶ [B] ──▶ [C] ──▶ [E] ──▶ NULL
                          ▲       ▲
                       Remove    Tail
```

After:
```
Head ──▶ [A] ──▶ [B] ──▶ [E] ──▶ NULL
                          ▲
                         Tail
```

## Time Slicing

### Time Slice Countdown

Each task gets 10 timer ticks (100ms) to run before being preempted:

```c
#define TIME_SLICE_TICKS 10  // At 100Hz, this is 100ms
```

**Countdown mechanism:**
```c
void scheduler_tick(void) {
    if (!current_task) return;
    
    // Every timer interrupt (every 10ms):
    current_task->time_slice--;     // 10 → 9 → 8 → ... → 0
    current_task->total_runtime++;  // Track CPU usage
    
    if (current_task->time_slice == 0) {
        // Time's up! Switch tasks
        // ... (context switch code) ...
        new_task->time_slice = TIME_SLICE_TICKS;  // Reset to 10
    }
}
```

### Timeline Example

```
Time (ms)    Tick    Current Task    time_slice    Action
──────────────────────────────────────────────────────────────
0            0       Idle            10            -
10           1       Idle            9             -
20           2       Idle            8             -
30           3       Idle            7             -
40           4       Idle            6             -
50           5       Idle            5             -
60           6       Idle            4             -
70           7       Idle            3             -
80           8       Idle            2             -
90           9       Idle            1             -
100          10      Idle            0             Switch → Shell
110          11      Shell           10            Reset time_slice
120          12      Shell           9             -
130          13      Shell           8             -
...
200          20      Shell           0             Switch → Idle
```

### Voluntary Yielding

A task can give up its time slice early:

```c
void task_yield(void) {
    if (!current_task) return;
    
    current_task->time_slice = 0;  // Force immediate switch
    scheduler_tick();              // Trigger scheduler
}
```

**Use cases:**
- Task is waiting for I/O
- Task has no work to do
- Cooperative multitasking

## API Reference

### Task Management

#### `task_t* task_create(const char* name, void (*entry_point)(void))`
Creates a new task.

**Parameters:**
- `name`: Human-readable task identifier (max 31 chars)
- `entry_point`: Function to run when task starts

**Returns:**
- Pointer to task structure, or NULL on failure

**Example:**
```c
void my_task_func(void) {
    while(1) {
        console_puts("Hello from task!\n");
        task_yield();
    }
}

task_t* my_task = task_create("mytask", my_task_func);
if (my_task) {
    scheduler_add_task(my_task);
}
```

#### `void task_destroy(task_t* task)`
Destroys a task and frees its resources.

**Parameters:**
- `task`: Task to destroy

**Note:** Task is automatically removed from scheduler.

#### `task_t* task_get_current(void)`
Returns the currently running task.

**Returns:**
- Pointer to current task, or NULL if scheduler not initialized

**Example:**
```c
task_t* me = task_get_current();
console_puts("I am: ");
console_puts(me->name);
console_putc('\n');
```

#### `void task_yield(void)`
Voluntarily give up CPU time.

**Effects:**
- Current task's time slice is set to 0
- Scheduler immediately picks next task
- Current task goes to back of queue

**Example:**
```c
void polite_task(void) {
    while(1) {
        do_quick_work();
        task_yield();  // Let others run
    }
}
```

#### `void task_block(void)`
Block current task (wait for I/O or event).

**Effects:**
- Task state becomes BLOCKED
- Task removed from ready queue
- Scheduler picks next task
- Task will NOT run until unblocked

**Example:**
```c
void io_task(void) {
    request_disk_read();
    task_block();  // Wait for I/O
    // ... resumes here after task_unblock() called ...
    process_data();
}
```

#### `void task_unblock(task_t* task)`
Unblock a previously blocked task.

**Parameters:**
- `task`: Task to unblock

**Effects:**
- Task state becomes READY
- Task added to ready queue
- Will run when scheduler picks it

**Example:**
```c
void disk_interrupt_handler(void) {
    task_t* waiting = get_waiting_task();
    task_unblock(waiting);  // I/O complete, resume task
}
```

#### `void task_print_list(void)`
Prints all tasks for debugging.

**Output:**
```
=== Task List ===
PID  Name            State      Runtime
-------------------------------------------
0    idle            RUNNING    12345 ticks
1    shell           READY      5678 ticks
2    network         BLOCKED    890 ticks

Current task: idle
```

### Scheduler Functions

#### `kerr_t scheduler_init(void)`
Initializes the scheduler and creates idle task.

**Returns:**
- `E_OK` on success
- `E_NOMEM` if idle task creation fails

**Note:** Must be called before `idt_enable_interrupts()`

#### `void scheduler_add_task(task_t* task)`
Adds a task to the ready queue.

**Parameters:**
- `task`: Task to add

**Effects:**
- Task state set to READY
- Task added to end of queue

#### `void scheduler_remove_task(task_t* task)`
Removes a task from the ready queue.

**Parameters:**
- `task`: Task to remove

**Note:** Does not destroy the task, just removes from queue

#### `void scheduler_tick(void)`
Called by PIT interrupt handler every 10ms.

**Effects:**
- Decrements current task's time slice
- Performs context switch if time slice expires
- Handles task state transitions

**Note:** Do not call manually - this is driven by hardware interrupts

### Context Switching

#### `void task_switch(cpu_state_t** old_context, cpu_state_t* new_context)`
Low-level context switch (implemented in assembly).

**Parameters:**
- `old_context`: Pointer to pointer where old RSP will be saved
- `new_context`: Stack pointer to restore

**Note:** This is called internally by the scheduler. Do not call directly.

## Usage Examples

### Example 1: Simple Task Creation

```c
void hello_task(void) {
    for (int i = 0; i < 10; i++) {
        console_puts("Hello from task!\n");
        task_yield();  // Be nice to other tasks
    }
    // Task exits automatically
}

void init(void) {
    task_init();
    scheduler_init();
    
    task_t* task = task_create("hello", hello_task);
    scheduler_add_task(task);
    
    idt_enable_interrupts();  // Start scheduling
}
```

### Example 2: Multiple Concurrent Tasks

```c
void counter_task(void) {
    int count = 0;
    while(1) {
        console_puts("Count: ");
        console_putint(count++);
        console_putc('\n');
        
        // Sleep for ~1 second (100 ticks)
        for (int i = 0; i < 100; i++) {
            task_yield();
        }
    }
}

void logger_task(void) {
    while(1) {
        log_system_status();
        task_yield();
    }
}

void init(void) {
    task_init();
    scheduler_init();
    
    scheduler_add_task(task_create("counter", counter_task));
    scheduler_add_task(task_create("logger", logger_task));
    
    idt_enable_interrupts();
}
```

### Example 3: Blocking I/O

```c
typedef struct {
    task_t* waiting_task;
    uint8_t* buffer;
    int complete;
} disk_request_t;

disk_request_t current_request;

void disk_read_task(void) {
    uint8_t buffer[512];
    
    console_puts("Starting disk read...\n");
    
    // Setup request
    current_request.waiting_task = task_get_current();
    current_request.buffer = buffer;
    current_request.complete = 0;
    
    // Initiate I/O
    start_disk_read(0, 0);  // Read LBA 0
    
    // Block until complete
    task_block();
    
    // Resumed after I/O complete
    console_puts("Disk read complete!\n");
    process_data(buffer);
}

void disk_interrupt_handler(void) {
    // I/O complete, copy data
    read_disk_data(current_request.buffer);
    current_request.complete = 1;
    
    // Wake up waiting task
    task_unblock(current_request.waiting_task);
}
```

### Example 4: Task Communication (Simple Message Passing)

```c
typedef struct {
    char message[256];
    int ready;
} mailbox_t;

mailbox_t shared_mailbox = {0};

void sender_task(void) {
    while(1) {
        strcpy(shared_mailbox.message, "Hello from sender!");
        shared_mailbox.ready = 1;
        
        // Wait for receiver to process
        while (shared_mailbox.ready) {
            task_yield();
        }
    }
}

void receiver_task(void) {
    while(1) {
        // Wait for message
        while (!shared_mailbox.ready) {
            task_yield();
        }
        
        // Process message
        console_puts("Received: ");
        console_puts(shared_mailbox.message);
        console_putc('\n');
        
        // Acknowledge
        shared_mailbox.ready = 0;
    }
}
```

## Debugging Tasks

### Using task_print_list()

```c
void cmd_ps(int argc, char** argv) {
    task_print_list();
}
```

Output:
```
=== Task List ===
PID  Name            State      Runtime
-------------------------------------------
0    idle            READY      45231 ticks
1    shell           RUNNING    12456 ticks
2    network         BLOCKED    234 ticks
3    logger          READY      5678 ticks

Current task: shell
```

### Serial Debugging

Enable verbose scheduler logging:

```c
#define DEBUG_SCHEDULER 1

void scheduler_tick(void) {
    #ifdef DEBUG_SCHEDULER
    serial_debug_puts("[SCHED] Tick ");
    serial_putint(pit_get_ticks());
    serial_debug_puts(" Task: ");
    serial_debug_puts(current_task->name);
    serial_debug_puts(" Slice: ");
    serial_putint(current_task->time_slice);
    serial_debug_puts("\n");
    #endif
    
    // ... rest of function ...
}
```

### Common Issues

#### 1. Triple Fault During Context Switch

**Symptom:** QEMU resets immediately after enabling interrupts

**Causes:**
- Stack corruption
- Incorrect register save/restore
- Misaligned stack pointer

**Debug:**
```c
// In task.asm, add debug output:
task_switch:
    ; Log context switch
    push rdi
    push rsi
    call log_context_switch
    pop rsi
    pop rdi
    ; ... rest of function ...
```

#### 2. Task Never Runs

**Symptom:** Task created but never executes

**Causes:**
- Forgot to add task to scheduler
- Task state not set to READY
- Interrupts not enabled

**Check:**
```c
task_t* task = task_create("mytask", func);
if (!task) {
    console_perror("Task creation failed!\n");
    return;
}
scheduler_add_task(task);  // Don't forget this!
task_print_list();  // Verify task is in queue
```

#### 3. Page Fault in Task

**Symptom:** Page fault when task runs

**Causes:**
- Stack overflow
- Invalid function pointer
- Accessing unmapped memory

**Debug:**
```c
// Check stack usage
void* stack_end = current_task->stack_base;
void* stack_current = get_rsp();
size_t used = stack_current - stack_end;

if (used > TASK_STACK_SIZE - 1024) {
    console_pwarn("Stack almost full!\n");
}
```

#### 4. Deadlock (All Tasks Blocked)

**Symptom:** System hangs, only idle task runs

**Causes:**
- All tasks blocked waiting for each other
- Forgot to unblock a task

**Prevention:**
```c
// Add timeout to blocking operations
void task_block_timeout(uint64_t max_ticks) {
    uint64_t start = pit_get_ticks();
    task_block();
    
    // Timeout handler (in scheduler_tick):
    if (task->state == BLOCKED) {
        if (pit_get_ticks() - task->block_time > task->timeout) {
            task_unblock(task);
            task->timeout_expired = 1;
        }
    }
}
```

## Performance Characteristics

### Time Complexity

| Operation | Time Complexity | Notes |
|-----------|----------------|-------|
| task_create() | O(1) | Constant allocation |
| scheduler_add_task() | O(1) | Append to linked list |
| scheduler_pick_next() | O(1) | Remove from head |
| scheduler_remove_task() | O(n) | Must search queue |
| task_switch() | O(1) | Fixed number of registers |
| scheduler_tick() | O(1) | Fixed operations |

### Memory Overhead

**Per Task:**
- Task structure: ~128 bytes
- Stack: 8 KB (configurable)
- **Total: ~8.1 KB per task**

**Maximum tasks:** 64 (configurable via MAX_TASKS)

**Total memory for 64 tasks:** ~520 KB

### Context Switch Time

Estimated context switch overhead:
- Save registers: ~10 instructions (~10 ns)
- Switch stack: 1 instruction (~1 ns)
- Restore registers: ~10 instructions (~10 ns)
- **Total: ~21 ns** (negligible compared to 10ms time slice)

## Future Enhancements

### Priority Scheduling

```c
typedef enum {
    PRIORITY_LOW = 0,
    PRIORITY_NORMAL = 1,
    PRIORITY_HIGH = 2,
    PRIORITY_REALTIME = 3
} task_priority_t;

typedef struct task {
    // ... existing fields ...
    task_priority_t priority;
    uint64_t dynamic_priority;  // Adjusted based on behavior
} task_t;
```

### SMP (Multi-CPU) Support

```c
typedef struct {
    uint8_t cpu_id;
    task_t* current_task;
    task_t* ready_queue_head;
    // ... per-CPU scheduler state ...
} cpu_scheduler_t;

cpu_scheduler_t cpu_schedulers[MAX_CPUS];
```

### Sleep/Timer Events

```c
void task_sleep(uint64_t ticks) {
    task_t* me = task_get_current();
    me->wakeup_time = pit_get_ticks() + ticks;
    task_block();
}

// In scheduler_tick():
if (task->state == BLOCKED && task->wakeup_time > 0) {
    if (pit_get_ticks() >= task->wakeup_time) {
        task_unblock(task);
    }
}
```

### Wait Queues

```c
typedef struct wait_queue {
    task_t* head;
    task_t* tail;
} wait_queue_t;

void wait_queue_add(wait_queue_t* wq, task_t* task) {
    // Add task to wait queue
    task_block();
}

void wait_queue_wake_all(wait_queue_t* wq) {
    while (wq->head) {
        task_t* task = wq->head;
        wq->head = task->next;
        task_unblock(task);
    }
}
```

## References

- [System V AMD64 ABI](https://gitlab.com/x86-psABIs/x86-64-ABI)
- Linux kernel scheduler ([kernel/sched/core.c](https://github.com/torvalds/linux/blob/master/kernel/sched/core.c))

---