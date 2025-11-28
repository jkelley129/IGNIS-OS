#include "tty.h"
#include "console/console.h"
#include "scheduler/task.h"
#include "libc/string.h"
#include "io/serial.h"

static tty_t global_tty;

kerr_t tty_init(void) {
    memset(&global_tty, 0, sizeof(global_tty));
    global_tty.read_pos = 0;
    global_tty.write_pos = 0;
    global_tty.count = 0;
    global_tty.waiting_task = NULL;
    global_tty.echo_enabled = 1;

    return E_OK;
}

tty_t* tty_get(void) {
    return &global_tty;
}

void tty_input_char(char c) {
    tty_t* tty = &global_tty;

    if (c == '\b') {
        if (tty->count > 0) {
            tty->write_pos = (tty->write_pos - 1 + TTY_BUFFER_SIZE) % TTY_BUFFER_SIZE;
            tty->count--;
            if (tty->echo_enabled) {
                console_backspace(1);
            }
        }
        return;
    }

    if (tty->echo_enabled && c != '\n') {
        console_putc(c);
    }

    //Add to buffer
    if (tty->count < TTY_BUFFER_SIZE) {
        tty->buffer[tty->write_pos] = c;
        tty->write_pos = (tty->write_pos + 1) % TTY_BUFFER_SIZE;
        tty->count++;
    }

    //If newline, wake up waiting task
    if (c == '\n') {
        if (tty->echo_enabled) console_putc('\n');

        if (tty->waiting_task) {
            serial_debug_puts("[TTY] Waking task: ");
            serial_debug_puts(tty->waiting_task->name);
            serial_debug_putc('\n');

            task_t* task_to_wake = tty->waiting_task;
            tty->waiting_task = NULL;
            task_unblock(task_to_wake);
        }
    }
}

size_t tty_read(char* buffer, size_t size) {
    tty_t* tty = &global_tty;
    task_t* current = task_get_current();

    serial_debug_puts("[TTY] Read called from task ");
    serial_debug_puts(current->name);
    serial_debug_putc('\n');

    //Block until newline
    while (1) {
        uint8_t has_newline = 0;
        size_t temp_pos = tty->read_pos;
        size_t checked = 0;

        while (checked < tty->count) {
            if (tty->buffer[temp_pos] == '\n') {
                has_newline = 1;
                break;
            }
            temp_pos = (temp_pos + 1) % TTY_BUFFER_SIZE;
            checked++;
        }

        if (has_newline) {
            break;
        }

        serial_debug_puts("[TTY] No complete line, blocking task\n");
        tty->waiting_task = current;
        task_block();

        serial_debug_puts("[TTY] Task woke up, checking for data\n");
    }

    //Read chars until newline or buffer full
    size_t bytes_read = 0;
    while (tty->count > 0 && bytes_read < size - 1) {
        char c = tty->buffer[tty->read_pos];
        buffer[bytes_read++] = c;

        tty->read_pos = (tty->read_pos + 1) % TTY_BUFFER_SIZE;
        tty->count--;

        if (c == '\n') {
            break;
        }
    }

    buffer[bytes_read] = '\0';

    serial_debug_puts("[TTY] Read ");
    char num_str[16];
    uitoa(bytes_read, num_str);
    serial_debug_puts(num_str);
    serial_debug_puts(" bytes\n");

    return bytes_read;
}