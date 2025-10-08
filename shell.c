#include "shell.h"
#include "io/vga.h"
#include "libc/string.h"
#include "drivers/pit.h"
#include "mm/memory.h"
#include "fs/vfs.h"

#define CMD_BUFFER_SIZE 256

static char cmd_buffer[CMD_BUFFER_SIZE];
static size_t cmd_pos = 0;  // Changed from uint32_t to size_t

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
        for (size_t i = 5; i < cmd_pos; i++) {
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
    vga_puts("Version: 0.0.01 (64-bit)\n");
    vga_puts("Developer: Josh Kelley\n");
    vga_puts("A hobby OS written from scratch!\n\n");
}

void cmd_uptime() {
    uint64_t ticks = pit_get_ticks();
    // At 100 Hz, each tick is 10ms, so 100 ticks = 1 second
    uint64_t total_seconds = ticks / 100;
    uint64_t hours = total_seconds / 3600;
    uint64_t minutes = (total_seconds % 3600) / 60;
    uint64_t seconds = total_seconds % 60;

    char num_str[32];

    vga_puts("\nSystem uptime: ");

    uitoa(hours, num_str);
    vga_puts(num_str);
    vga_puts("h ");

    uitoa(minutes, num_str);
    vga_puts(num_str);
    vga_puts("m ");

    uitoa(seconds, num_str);
    vga_puts(num_str);
    vga_puts("s\n\n");
}

void cmd_ticks() {
    uint64_t ticks = pit_get_ticks();
    char num_str[32];

    vga_puts("\nPIT ticks: ");
    uitoa(ticks, num_str);
    vga_puts(num_str);
    vga_puts("\n\n");
}

void cmd_meminfo(){
    memory_print_stats();
}

void cmd_memtest() {
    vga_puts("\n=== Memory Allocator Test ===\n");

    vga_puts("Allocating 3 blocks...\n");
    void* ptr1 = kmalloc(64);
    void* ptr2 = kmalloc(128);
    void* ptr3 = kmalloc(256);

    if (ptr1 && ptr2 && ptr3) {
        vga_puts("Success! Allocated 3 blocks\n");

        vga_puts("Freeing middle block...\n");
        kfree(ptr2);

        vga_puts("Allocating same size again...\n");
        void* ptr4 = kmalloc(128);

        if (ptr4) {
            vga_puts("Success! Reused freed block\n");
        }

        vga_puts("Testing kcalloc...\n");
        void* ptr5 = kcalloc(10, 8);
        if (ptr5) {
            vga_puts("Success! Allocated zeroed memory\n");
            kfree(ptr5);
        }

        vga_puts("Cleaning up...\n");
        kfree(ptr1);
        kfree(ptr3);
        kfree(ptr4);

        vga_puts("Test complete!\n\n");
    } else {
        vga_puts("ERROR: Allocation failed!\n\n");
    }
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
    } else if (strcmp(cmd_buffer, "uptime") == 0) {
        cmd_uptime();
    } else if (strcmp(cmd_buffer, "ticks") == 0) {
        cmd_ticks();
    } else if(strcmp(cmd_buffer, "meminfo") == 0) {
        cmd_meminfo();
    } else if(strcmp(cmd_buffer, "memtest") == 0) {
        cmd_memtest();
    } else if(strcmp(cmd_buffer, "kmalloc") == 0) {
        kmalloc(1024);
        vga_putc('\n');
    } else if(strcmp(cmd_buffer, "ls") == 0) {
        vga_putc('\n');
        vfs_list("/");
    } else if(strcmp(cmd_buffer, "tree") == 0) {
        vga_putc('\n');
        vfs_print_tree(vfs_resolve_path("/"), 1);
    } else if(strcmp(cmd_buffer, "touch") == 0) {
        vfs_create_file("/my_file");
        vga_putc('\n');
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