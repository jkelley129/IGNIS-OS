#ifndef TTY_H
#define TTY_H

#include "libc/stdint.h"
#include "libc/stddef.h"
#include "error_handling/errno.h"
#include "scheduler/task.h"

#define TTY_BUFFER_SIZE 256

typedef struct {
    char buffer[TTY_BUFFER_SIZE];
    size_t read_pos;
    size_t write_pos;
    size_t count;
    task_t* waiting_task;  // Task blocked on read
    uint8_t echo_enabled;
} tty_t;

// Initialize TTY system
kerr_t tty_init(void);

// Get the global TTY instance
tty_t* tty_get(void);

// Called by keyboard driver when key is pressed
void tty_input_char(char c);

// Blocking read
// Returns number of characters read (will block until Enter is pressed)
size_t tty_read(char* buffer, size_t size);

// Non-blocking write to console
void tty_write(const char* str, size_t len);

#endif