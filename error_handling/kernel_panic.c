#include "kernel_panic.h"
#include "console/console.h"
#include "io/serial.h"
#include "interrupts/idt.h"
#include "libc/string.h"
#include "drivers/pit.h"
#include "mm/pmm.h"
#include "mm/memory_layout.h"

// Panic state to prevent recursive panics
static volatile int panic_in_progress = 0;

// Direct VGA access for panic screen (in case console fails)
static volatile uint16_t* vga_mem = (uint16_t*)PHYS_TO_VIRT(0xB8000);
static int vga_row = 0;
static int vga_col = 0;

// Color definitions for panic screen
#define PANIC_BG CONSOLE_COLOR_BLUE
#define PANIC_FG CONSOLE_COLOR_WHITE
#define PANIC_HEADER_FG CONSOLE_COLOR_LIGHT_CYAN
#define PANIC_ERROR_FG CONSOLE_COLOR_LIGHT_RED

// Direct VGA write function (bypasses console in case it's broken)
static void panic_vga_putc(char c, uint8_t fg, uint8_t bg) {
    if (c == '\n') {
        vga_col = 0;
        vga_row++;
        if (vga_row >= 25) vga_row = 24;
        return;
    }

    if (vga_row >= 25) vga_row = 24;
    if (vga_col >= 80) {
        vga_col = 0;
        vga_row++;
    }

    int pos = vga_row * 80 + vga_col;
    vga_mem[pos] = (bg << 12) | (fg << 8) | c;
    vga_col++;
}

static void panic_vga_puts(const char* str, uint8_t fg, uint8_t bg) {
    while (*str) {
        panic_vga_putc(*str, fg, bg);
        str++;
    }
}

static void panic_vga_clear(uint8_t fg, uint8_t bg) {
    uint16_t blank = (bg << 12) | (fg << 8) | ' ';
    for (int i = 0; i < 80 * 25; i++) {
        vga_mem[i] = blank;
    }
    vga_row = 0;
    vga_col = 0;
}

static void panic_print_header(void) {
    panic_vga_puts("\n", PANIC_FG, PANIC_BG);
    panic_vga_puts("  ========================================\n", PANIC_HEADER_FG, PANIC_BG);
    panic_vga_puts("  ||                                    ||\n", PANIC_HEADER_FG, PANIC_BG);
    panic_vga_puts("  ||      KERNEL PANIC - IGNIS OS       ||\n", PANIC_HEADER_FG, PANIC_BG);
    panic_vga_puts("  ||                                    ||\n", PANIC_HEADER_FG, PANIC_BG);
    panic_vga_puts("  ========================================\n", PANIC_HEADER_FG, PANIC_BG);
    panic_vga_puts("\n", PANIC_FG, PANIC_BG);
}

static void panic_print_system_state(void) {
    char num_str[32];

    // Get system uptime
    uint64_t ticks = pit_get_ticks();
    uint64_t seconds = ticks / 100;
    uint64_t hours = seconds / 3600;
    uint64_t minutes = (seconds % 3600) / 60;
    uint64_t secs = seconds % 60;

    panic_vga_puts("  System Uptime: ", PANIC_HEADER_FG, PANIC_BG);
    uitoa(hours, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts("h ", PANIC_FG, PANIC_BG);
    uitoa(minutes, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts("m ", PANIC_FG, PANIC_BG);
    uitoa(secs, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts("s\n", PANIC_FG, PANIC_BG);

    // Memory info
    panic_vga_puts("  Free Memory:   ", PANIC_HEADER_FG, PANIC_BG);
    uitoa(pmm_get_free_memory() / 1024, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts(" KB / ", PANIC_FG, PANIC_BG);
    uitoa(pmm_get_total_memory() / 1024, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts(" KB\n", PANIC_FG, PANIC_BG);

    panic_vga_puts("\n", PANIC_FG, PANIC_BG);
}

static void panic_print_registers(stack_frame_t* frame) {
    char num_str[32];

    panic_vga_puts("  Register Dump:\n", PANIC_HEADER_FG, PANIC_BG);

    panic_vga_puts("    RBP: 0x", PANIC_FG, PANIC_BG);
    uitoa(frame->rbp, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts("    RSP: 0x", PANIC_FG, PANIC_BG);
    uitoa(frame->rsp, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts("\n", PANIC_FG, PANIC_BG);

    panic_vga_puts("    RIP: 0x", PANIC_FG, PANIC_BG);
    uitoa(frame->rip, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts("\n", PANIC_FG, PANIC_BG);

    panic_vga_puts("    CR2: 0x", PANIC_FG, PANIC_BG);
    uitoa(frame->cr2, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts("    CR3: 0x", PANIC_FG, PANIC_BG);
    uitoa(frame->cr3, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts("\n\n", PANIC_FG, PANIC_BG);
}

static void panic_log_to_serial(const char* message, const char* file,
                                 int line, const char* function) {
    serial_debug_puts("\n\n");
    serial_debug_puts("*** KERNEL PANIC ***\n");
    serial_debug_puts("Message: ");
    serial_debug_puts(message);
    serial_debug_puts("\n");

    if (file) {
        serial_debug_puts("File: ");
        serial_debug_puts(file);
        serial_debug_puts("\n");

        serial_debug_puts("Line: ");
        char num_str[32];
        uitoa(line, num_str);
        serial_debug_puts(num_str);
        serial_debug_puts("\n");
    }

    if (function) {
        serial_debug_puts("Function: ");
        serial_debug_puts(function);
        serial_debug_puts("\n");
    }

    serial_debug_puts("\n");
}

void get_stack_frame(stack_frame_t* frame) {
    if (!frame) return;

    // Get register values
    asm volatile("mov %%rbp, %0" : "=r"(frame->rbp));
    asm volatile("mov %%rsp, %0" : "=r"(frame->rsp));

    // Get instruction pointer (approximate)
    frame->rip = (uint64_t)__builtin_return_address(0);

    // Get control registers
    asm volatile("mov %%cr2, %0" : "=r"(frame->cr2));
    asm volatile("mov %%cr3, %0" : "=r"(frame->cr3));
}

void kernel_panic(const char* message) {
    kernel_panic_with_context(message, 0, 0, 0);
}

void kernel_panic_with_error(const char* message, int error_code) {
    // Disable interrupts immediately
    idt_disable_interrupts();

    // Prevent recursive panics
    if (panic_in_progress) {
        serial_debug_puts("RECURSIVE PANIC DETECTED!\n");
        while(1) asm volatile("hlt");
    }
    panic_in_progress = 1;

    // Use direct VGA access
    panic_vga_clear(PANIC_FG, PANIC_BG);

    // Print panic header
    panic_print_header();

    // Print error message
    panic_vga_puts("  ERROR: ", PANIC_ERROR_FG, PANIC_BG);
    panic_vga_puts(message, PANIC_ERROR_FG, PANIC_BG);
    panic_vga_puts("\n", PANIC_FG, PANIC_BG);

    panic_vga_puts("  Error Code: ", PANIC_FG, PANIC_BG);
    char num_str[32];
    uitoa(error_code, num_str);
    panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
    panic_vga_puts("\n\n", PANIC_FG, PANIC_BG);

    // Print system state
    panic_print_system_state();

    // Get and print registers
    stack_frame_t frame;
    get_stack_frame(&frame);
    panic_print_registers(&frame);

    // Footer
    panic_vga_puts("  ========================================\n", PANIC_HEADER_FG, PANIC_BG);
    panic_vga_puts("  System halted. Please reboot.\n", PANIC_HEADER_FG, PANIC_BG);
    panic_vga_puts("  ========================================\n", PANIC_HEADER_FG, PANIC_BG);

    // Log to serial
    panic_log_to_serial(message, 0, 0, 0);
    serial_debug_puts("Error code: ");
    uitoa(error_code, num_str);
    serial_debug_puts(num_str);
    serial_debug_puts("\n");

    // Halt forever
    while(1) {
        asm volatile("hlt");
    }
}

void kernel_panic_with_context(const char* message, const char* file,
                                int line, const char* function) {
    // Disable interrupts immediately
    idt_disable_interrupts();

    // Prevent recursive panics
    if (panic_in_progress) {
        serial_debug_puts("RECURSIVE PANIC DETECTED!\n");
        while(1) asm volatile("hlt");
    }
    panic_in_progress = 1;

    // Log to serial first (in case screen output fails)
    panic_log_to_serial(message, file, line, function);

    // Use direct VGA access to ensure output works
    panic_vga_clear(PANIC_FG, PANIC_BG);

    // Print panic header
    panic_print_header();

    // Print error message
    panic_vga_puts("  ERROR: ", PANIC_ERROR_FG, PANIC_BG);
    panic_vga_puts(message, PANIC_ERROR_FG, PANIC_BG);
    panic_vga_puts("\n\n", PANIC_FG, PANIC_BG);

    // Print location information if available
    if (file || function) {
        panic_vga_puts("  Location:\n", PANIC_HEADER_FG, PANIC_BG);

        if (file) {
            panic_vga_puts("    File: ", PANIC_FG, PANIC_BG);
            panic_vga_puts(file, PANIC_FG, PANIC_BG);
            panic_vga_puts("\n", PANIC_FG, PANIC_BG);

            if (line > 0) {
                panic_vga_puts("    Line: ", PANIC_FG, PANIC_BG);
                char num_str[32];
                uitoa(line, num_str);
                panic_vga_puts(num_str, PANIC_FG, PANIC_BG);
                panic_vga_puts("\n", PANIC_FG, PANIC_BG);
            }
        }

        if (function) {
            panic_vga_puts("    Function: ", PANIC_FG, PANIC_BG);
            panic_vga_puts(function, PANIC_FG, PANIC_BG);
            panic_vga_puts("\n", PANIC_FG, PANIC_BG);
        }

        panic_vga_puts("\n", PANIC_FG, PANIC_BG);
    }

    // Print system state
    panic_print_system_state();

    // Get and print registers
    stack_frame_t frame;
    get_stack_frame(&frame);
    panic_print_registers(&frame);

    // Footer
    panic_vga_puts("  ========================================\n", PANIC_HEADER_FG, PANIC_BG);
    panic_vga_puts("  System halted. Please reboot.\n", PANIC_HEADER_FG, PANIC_BG);
    panic_vga_puts("  ========================================\n", PANIC_HEADER_FG, PANIC_BG);

    // Halt forever
    while(1) {
        asm volatile("hlt");
    }
}