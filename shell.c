#include "shell.h"
#include "io/vga.h"
#include "libc/string.h"

#define CMD_BUFFER_SIZE 256

static char cmd_buffer[CMD_BUFFER_SIZE];
static uint32_t cmd_pos = 0;

void shell_init(){
    memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
    cmd_pos = 0;
    shell_print_prompt();
}

void shell_print_prompt(){
    vga_set_color((vga_color_attr_t){VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK});
    vga_puts("kernel@ignis ");
    vga_puts(">>> ");
}

// Command handlers
void cmd_help() {
    vga_puts("\nAvailable commands:\n");
    vga_puts("  help  - Show this message\n");
    vga_puts("  clear - Clear the screen\n");
    vga_puts("  echo  - Print text\n");
    vga_puts("  about - About IGNIS\n");
    vga_puts("\n");
}

void cmd_clear(){
    vga_clear();
}

void cmd_echo() {
    // Print everything after "echo "
    if (cmd_pos > 5) {
        vga_putc('\n');
        for (uint32_t i = 5; i < cmd_pos; i++) {
            vga_putc(cmd_buffer[i]);
        }
        vga_putc('\n');
    } else {
        vga_puts("\n");
    }
}

void cmd_about() {
    vga_puts("\n");
    vga_set_color((vga_color_attr_t){VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK});
    vga_puts("IGNIS Operating System\n");
    vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    vga_puts("Version: 0.0.01\n");
    vga_puts("Developer: Josh Kelley\n");
    vga_puts("A hobby OS written from scratch!\n\n");
}

void shell_execute_command(){
    cmd_buffer[cmd_pos] = '\0';

    if(cmd_pos == 0){
        vga_putc('\n');
        shell_print_prompt();
        return;
    }

    // Parse and execute command
    if (strcmp(cmd_buffer, "help") == 0) {
        cmd_help();
    } else if (strcmp(cmd_buffer, "clear") == 0) {
        cmd_clear();
    } else if (strncmp(cmd_buffer, "echo ", 5) == 0 || strcmp(cmd_buffer, "echo") == 0) {
        cmd_echo();
    } else if (strcmp(cmd_buffer, "about") == 0) {
        cmd_about();
    } else {
        vga_puts("\nUnknown command: ");
        vga_puts(cmd_buffer);
        vga_puts("\nType 'help' for available commands.\n");
    }

    // Reset buffer and print new prompt
    memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
    cmd_pos = 0;
    shell_print_prompt();
}

void shell_handle_char(char c) {
    if (c == '\n') {
        shell_execute_command();
    } else if (c == '\b') {
        if (cmd_pos > 0) {
            cmd_pos--;
            cmd_buffer[cmd_pos] = '\0';
        }
    } else {
        if (cmd_pos < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_pos++] = c;
        }
    }
}