#include "shell.h"
#include "io/vga.h"
#include "libc/string.h"
#include "drivers/pit.h"
#include "drivers/block.h"
#include "mm/memory.h"
#include "fs/vfs.h"
#include "../error_handling/errno.h"

#define CMD_BUFFER_SIZE 256

static char cmd_buffer[CMD_BUFFER_SIZE];
static size_t cmd_pos = 0;

// Command registry
static const shell_command_t commands[] = {
        {"help", "Display available commands", cmd_help},
        {"clear", "Clear the screen", cmd_clear},
        {"echo", "Print text to screen", cmd_echo},
        {"about", "About IGNIS OS", cmd_about},
        {"uptime", "Show system uptime", cmd_uptime},
        {"ticks", "Show PIT tick count", cmd_ticks},
        {"meminfo", "Display memory statistics", cmd_meminfo},
        {"memtest", "Run memory allocator test", cmd_memtest},
        {"ls", "List directory contents", cmd_ls},
        {"tree", "Display directory tree", cmd_tree},
        {"touch", "Create a new file", cmd_touch},
        {"mkdir", "Create a new directory", cmd_mkdir},
        {"rm", "Remove a file or directory", cmd_rm},
        {"cat", "Display file contents", cmd_cat},
        {"write", "Write data to a file", cmd_write},
        {"cp", "Copy a file", cmd_cp},
        {"lsblk", "List block devices", cmd_lsblk},
        {"blkread", "Read from block device", cmd_blkread},
        {"blkwrite", "Write to block device", cmd_blkwrite},
        {"blktest", "Test block device I/O", cmd_blktest},
        {"hexdump", "Display file in hexadecimal", cmd_hexdump},
        {0, 0, 0} // Sentinel
};

// Parse command line into argc/argv
static int parse_command(char* input, char** argv, int max_args) {
    int argc = 0;
    int in_word = 0;

    for (size_t i = 0; input[i] && argc < max_args; i++) {
        if (input[i] == ' ' || input[i] == '\t') {
            if (in_word) {
                input[i] = '\0';
                in_word = 0;
            }
        } else {
            if (!in_word) {
                argv[argc++] = &input[i];
                in_word = 1;
            }
        }
    }

    return argc;
}

void shell_init() {
    memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
    cmd_pos = 0;
    shell_print_prompt();
}

void shell_print_prompt() {
    vga_set_color((vga_color_attr_t){VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK});
    vga_puts("ignis");
    vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    vga_puts("$ ");
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void cmd_help(int argc, char** argv) {
    vga_puts("\n");
    vga_set_color((vga_color_attr_t){VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK});
    vga_puts("IGNIS Shell - Available Commands\n");
    vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    vga_puts("================================\n\n");

    for (int i = 0; commands[i].name; i++) {
        vga_set_color((vga_color_attr_t){VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK});
        vga_puts("  ");
        vga_puts(commands[i].name);
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});

        // Padding
        size_t len = strlen(commands[i].name);
        for (size_t j = len; j < 12; j++) {
            vga_putc(' ');
        }

        vga_puts(commands[i].description);
        vga_putc('\n');
    }
    vga_putc('\n');
}

void cmd_clear(int argc, char** argv) {
    vga_clear();
}

void cmd_echo(int argc, char** argv) {
    vga_putc('\n');
    for (int i = 1; i < argc; i++) {
        vga_puts(argv[i]);
        if (i < argc - 1) vga_putc(' ');
    }
    vga_puts("\n\n");
}

void cmd_about(int argc, char** argv) {
    vga_puts("\n");
    vga_set_color((vga_color_attr_t){VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK});
    vga_puts("╔════════════════════════════════════╗\n");
    vga_puts("║       IGNIS Operating System       ║\n");
    vga_puts("╚════════════════════════════════════╝\n");
    vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    vga_puts("\n");
    vga_puts("Version:     0.0.01 (64-bit)\n");
    vga_puts("Developer:   Josh Kelley\n");
    vga_puts("License:     Apache 2.0\n");
    vga_puts("Description: A hobby OS written from scratch\n");
    vga_puts("\n");
    vga_puts("Features:\n");
    vga_puts("  • VGA text mode output\n");
    vga_puts("  • Interrupt handling (IDT)\n");
    vga_puts("  • Keyboard driver\n");
    vga_puts("  • PIT timer\n");
    vga_puts("  • Memory allocator\n");
    vga_puts("  • In-memory filesystem (VFS)\n");
    vga_puts("  • Block device layer\n");
    vga_puts("  • ATA disk driver\n");
    vga_puts("  • NVMe disk driver\n");
    vga_puts("\n");
}

void cmd_uptime(int argc, char** argv) {
    uint64_t ticks = pit_get_ticks();
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

void cmd_ticks(int argc, char** argv) {
    uint64_t ticks = pit_get_ticks();
    char num_str[32];

    vga_puts("\nPIT ticks: ");
    uitoa(ticks, num_str);
    vga_puts(num_str);
    vga_puts("\n\n");
}

void cmd_meminfo(int argc, char** argv) {
    memory_print_stats();
}

void cmd_memtest(int argc, char** argv) {
    vga_puts("\n=== Memory Allocator Test ===\n");

    vga_puts("Allocating 3 blocks (64, 128, 256 bytes)...\n");
    void* ptr1 = kmalloc(64);
    void* ptr2 = kmalloc(128);
    void* ptr3 = kmalloc(256);

    if (ptr1 && ptr2 && ptr3) {
        vga_set_color((vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});
        vga_puts("✓ Allocation successful\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});

        vga_puts("Freeing middle block...\n");
        kfree(ptr2);

        vga_puts("Reallocating 128 bytes...\n");
        void* ptr4 = kmalloc(128);

        if (ptr4) {
            vga_set_color((vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});
            vga_puts("✓ Reused freed block\n");
            vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
        }

        vga_puts("Testing kcalloc (10 * 8 bytes)...\n");
        void* ptr5 = kcalloc(10, 8);
        if (ptr5) {
            uint8_t* data = (uint8_t*)ptr5;
            int all_zero = 1;
            for (int i = 0; i < 80; i++) {
                if (data[i] != 0) {
                    all_zero = 0;
                    break;
                }
            }

            if (all_zero) {
                vga_set_color((vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});
                vga_puts("✓ Memory properly zeroed\n");
                vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
            }
            kfree(ptr5);
        }

        vga_puts("Cleaning up...\n");
        kfree(ptr1);
        kfree(ptr3);
        kfree(ptr4);

        vga_set_color((vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});
        vga_puts("✓ Test complete!\n\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    } else {
        vga_set_color((vga_color_attr_t){VGA_COLOR_RED, VGA_COLOR_BLACK});
        vga_puts("✗ Allocation failed!\n\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    }
}

void cmd_ls(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";
    vga_putc('\n');
    vfs_list(path);
    vga_putc('\n');
}

void cmd_tree(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";
    file_t* dir = vfs_resolve_path(path);

    if (!dir) {
        vga_perror("Directory not found\n");
        return;
    }

    vga_putc('\n');
    vfs_print_tree(dir, 0);
    vga_putc('\n');
}

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        vga_perror("Usage: touch <filename>\n");
        return;
    }

    file_t* file = vfs_create_file(argv[1]);
    if (file) {
        vga_puts("Created file: ");
        vga_puts(argv[1]);
        vga_putc('\n');
    } else {
        vga_perror("Failed to create file\n");
    }
}

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        vga_perror("Usage: mkdir <dirname>\n");
        return;
    }

    file_t* dir = vfs_create_directory(argv[1]);
    if (dir) {
        vga_puts("Created directory: ");
        vga_puts(argv[1]);
        vga_putc('\n');
    } else {
        vga_perror("Failed to create directory\n");
    }
}

void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        vga_perror("Usage: rm <path>\n");
        return;
    }

    if (vfs_delete(argv[1]) == 0) {
        vga_puts("Removed: ");
        vga_puts(argv[1]);
        vga_putc('\n');
    } else {
        vga_perror("Failed to remove file\n");
    }
}

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        vga_perror("Usage: cat <filename>\n");
        return;
    }

    file_t* file = vfs_open(argv[1]);
    if (!file) {
        vga_perror("File not found\n");
        return;
    }

    if (file->type != FILE_TYPE_REGULAR) {
        vga_perror("Not a regular file\n");
        return;
    }

    vga_putc('\n');
    for (size_t i = 0; i < file->size; i++) {
        vga_putc(file->data[i]);
    }
    vga_puts("\n\n");
}

void cmd_write(int argc, char** argv) {
    if (argc < 3) {
        vga_perror("Usage: write <filename> <text>\n");
        return;
    }

    file_t* file = vfs_open(argv[1]);
    if (!file) {
        file = vfs_create_file(argv[1]);
    }

    if (!file) {
        vga_perror("Failed to open/create file\n");
        return;
    }

    // Concatenate all arguments after filename
    char buffer[256];
    size_t pos = 0;
    for (int i = 2; i < argc && pos < 255; i++) {
        size_t len = strlen(argv[i]);
        for (size_t j = 0; j < len && pos < 255; j++) {
            buffer[pos++] = argv[i][j];
        }
        if (i < argc - 1 && pos < 255) {
            buffer[pos++] = ' ';
        }
    }

    if (vfs_write(file, buffer, pos) >= 0) {
        vga_puts("Wrote ");
        char num_str[32];
        uitoa(pos, num_str);
        vga_puts(num_str);
        vga_puts(" bytes to ");
        vga_puts(argv[1]);
        vga_putc('\n');
    } else {
        vga_perror("Write failed\n");
    }
}

void cmd_cp(int argc, char** argv) {
    if (argc < 3) {
        vga_perror("Usage: cp <source> <dest>\n");
        return;
    }

    file_t* src = vfs_open(argv[1]);
    if (!src) {
        vga_perror("Source file not found\n");
        return;
    }

    if (vfs_copy_file(argv[2], src, src->size) == 0) {
        vga_puts("Copied ");
        vga_puts(argv[1]);
        vga_puts(" to ");
        vga_puts(argv[2]);
        vga_putc('\n');
    } else {
        vga_perror("Copy failed\n");
    }
}

void cmd_lsblk(int argc, char** argv) {
    block_list_devices();
}

void cmd_blkread(int argc, char** argv) {
    if (argc < 3) {
        vga_perror("Usage: blkread <device_id> <lba>\n");
        return;
    }

    // Parse device ID
    uint8_t dev_id = 0;
    for (size_t i = 0; argv[1][i]; i++) {
        if (argv[1][i] >= '0' && argv[1][i] <= '9') {
            dev_id = dev_id * 10 + (argv[1][i] - '0');
        }
    }

    // Parse LBA
    uint64_t lba = 0;
    for (size_t i = 0; argv[2][i]; i++) {
        if (argv[2][i] >= '0' && argv[2][i] <= '9') {
            lba = lba * 10 + (argv[2][i] - '0');
        }
    }

    uint8_t* buffer = kmalloc(512);
    if (!buffer) {
        vga_perror("Failed to allocate buffer\n");
        return;
    }

    vga_puts("\nReading device ");
    char num_str[32];
    uitoa(dev_id, num_str);
    vga_puts(num_str);
    vga_puts(", LBA ");
    uitoa(lba, num_str);
    vga_puts(num_str);
    vga_puts("...\n");

    if (block_read(dev_id, lba, buffer) == 0) {
        vga_set_color((vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});
        vga_puts("✓ Read successful\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});

        // Display first 64 bytes
        vga_puts("\nFirst 64 bytes:\n");
        for (int i = 0; i < 64; i++) {
            if (i % 16 == 0) {
                vga_puts("\n");
                uitoa(i, num_str);
                vga_puts(num_str);
                vga_puts(": ");
            }

            // Print hex byte
            uint8_t byte = buffer[i];
            char hex[3];
            hex[0] = "0123456789ABCDEF"[byte >> 4];
            hex[1] = "0123456789ABCDEF"[byte & 0xF];
            hex[2] = '\0';
            vga_puts(hex);
            vga_putc(' ');
        }
        vga_puts("\n\n");
    } else {
        vga_set_color((vga_color_attr_t){VGA_COLOR_RED, VGA_COLOR_BLACK});
        vga_puts("✗ Read failed\n\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    }

    kfree(buffer);
}

void cmd_blkwrite(int argc, char** argv) {
    if (argc < 4) {
        vga_perror("Usage: blkwrite <device_id> <lba> <data>\n");
        return;
    }

    // Parse device ID
    uint8_t dev_id = 0;
    for (size_t i = 0; argv[1][i]; i++) {
        if (argv[1][i] >= '0' && argv[1][i] <= '9') {
            dev_id = dev_id * 10 + (argv[1][i] - '0');
        }
    }

    // Parse LBA
    uint64_t lba = 0;
    for (size_t i = 0; argv[2][i]; i++) {
        if (argv[2][i] >= '0' && argv[2][i] <= '9') {
            lba = lba * 10 + (argv[2][i] - '0');
        }
    }

    uint8_t* buffer = kmalloc(512);
    if (!buffer) {
        vga_perror("Failed to allocate buffer\n");
        return;
    }

    memset(buffer, 0, 512);

    // Copy data to buffer
    size_t len = strlen(argv[3]);
    if (len > 512) len = 512;
    for (size_t i = 0; i < len; i++) {
        buffer[i] = argv[3][i];
    }

    vga_puts("\nWriting to device ");
    char num_str[32];
    uitoa(dev_id, num_str);
    vga_puts(num_str);
    vga_puts(", LBA ");
    uitoa(lba, num_str);
    vga_puts(num_str);
    vga_puts("...\n");

    kerr_t blk_write_status = block_write(dev_id, lba, buffer);

    if (blk_write_status == 0) {
        vga_set_color((vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});
        vga_puts("Write successful\n\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    } else {
        vga_set_color((vga_color_attr_t){VGA_COLOR_RED, VGA_COLOR_BLACK});
        vga_puts("Write failed with error");
        k_strerror(blk_write_status);
        vga_puts(k_strerror(blk_write_status));
        vga_puts("\n\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    }

    kfree(buffer);
}

void cmd_blktest(int argc, char** argv) {
    if (argc < 2) {
        vga_perror("Usage: blktest <device_id>\n");
        return;
    }

    // Parse device ID
    uint8_t dev_id = 0;
    for (size_t i = 0; argv[1][i]; i++) {
        if (argv[1][i] >= '0' && argv[1][i] <= '9') {
            dev_id = dev_id * 10 + (argv[1][i] - '0');
        }
    }

    vga_puts("\n=== Block Device Test ===\n");

    block_device_t* dev = block_get_device(dev_id);
    if (!dev || !dev->present) {
        vga_perror("Device not found\n");
        return;
    }

    vga_puts("Testing device ");
    char num_str[32];
    uitoa(dev_id, num_str);
    vga_puts(num_str);
    vga_puts(" (");
    vga_puts(dev->label);
    vga_puts(")\n\n");

    uint8_t* buffer = kmalloc(512);
    if (!buffer) {
        vga_perror("Failed to allocate buffer\n");
        return;
    }

    // Write test pattern
    vga_puts("Writing test pattern...\n");
    for (int i = 0; i < 512; i++) {
        buffer[i] = i & 0xFF;
    }

    if (block_write(dev_id, 100, buffer) != 0) {
        vga_set_color((vga_color_attr_t){VGA_COLOR_RED, VGA_COLOR_BLACK});
        vga_puts("✗ Write failed\n\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
        kfree(buffer);
        return;
    }

    vga_set_color((vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});
    vga_puts("✓ Write successful\n");
    vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});

    // Clear buffer
    memset(buffer, 0, 512);

    // Read back
    vga_puts("Reading back data...\n");
    if (block_read(dev_id, 100, buffer) != 0) {
        vga_set_color((vga_color_attr_t){VGA_COLOR_RED, VGA_COLOR_BLACK});
        vga_puts("✗ Read failed\n\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
        kfree(buffer);
        return;
    }

    vga_set_color((vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});
    vga_puts("✓ Read successful\n");
    vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});

    // Verify
    vga_puts("Verifying data...\n");
    int errors = 0;
    for (int i = 0; i < 512; i++) {
        if (buffer[i] != (i & 0xFF)) {
            errors++;
        }
    }

    if (errors == 0) {
        vga_set_color((vga_color_attr_t){VGA_COLOR_GREEN, VGA_COLOR_BLACK});
        vga_puts("✓ Verification passed!\n\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    } else {
        vga_set_color((vga_color_attr_t){VGA_COLOR_RED, VGA_COLOR_BLACK});
        vga_puts("✗ Verification failed! Errors: ");
        uitoa(errors, num_str);
        vga_puts(num_str);
        vga_puts("\n\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
    }

    kfree(buffer);
}

void cmd_hexdump(int argc, char** argv) {
    if (argc < 2) {
        vga_perror("Usage: hexdump <filename>\n");
        return;
    }

    file_t* file = vfs_open(argv[1]);
    if (!file) {
        vga_perror("File not found\n");
        return;
    }

    if (file->type != FILE_TYPE_REGULAR) {
        vga_perror("Not a regular file\n");
        return;
    }

    vga_puts("\nHex dump of ");
    vga_puts(argv[1]);
    vga_puts(":\n\n");

    for (size_t i = 0; i < file->size; i++) {
        if (i % 16 == 0) {
            char num_str[32];
            uitoa(i, num_str);

            // Pad address
            size_t len = strlen(num_str);
            for (size_t j = len; j < 4; j++) {
                vga_putc('0');
            }
            vga_puts(num_str);
            vga_puts(": ");
        }

        uint8_t byte = file->data[i];
        char hex[3];
        hex[0] = "0123456789ABCDEF"[byte >> 4];
        hex[1] = "0123456789ABCDEF"[byte & 0xF];
        hex[2] = '\0';
        vga_puts(hex);
        vga_putc(' ');

        if ((i + 1) % 16 == 0) {
            vga_puts("  |");
            for (size_t j = i - 15; j <= i; j++) {
                char c = file->data[j];
                if (c >= 32 && c <= 126) {
                    vga_putc(c);
                } else {
                    vga_putc('.');
                }
            }
            vga_puts("|\n");
        }
    }

    // Print remaining ASCII if not aligned
    if (file->size % 16 != 0) {
        size_t remaining = file->size % 16;
        for (size_t i = remaining; i < 16; i++) {
            vga_puts("   ");
        }
        vga_puts("  |");
        for (size_t i = file->size - remaining; i < file->size; i++) {
            char c = file->data[i];
            if (c >= 32 && c <= 126) {
                vga_putc(c);
            } else {
                vga_putc('.');
            }
        }
        for (size_t i = remaining; i < 16; i++) {
            vga_putc(' ');
        }
        vga_puts("|\n");
    }

    vga_puts("\n");
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

void shell_execute_command() {
    cmd_buffer[cmd_pos] = '\0';

    if (cmd_pos == 0) {
        vga_putc('\n');
        shell_print_prompt();
        return;
    }

    // Parse command into argc/argv
    char* argv[MAX_ARGS];
    int argc = parse_command(cmd_buffer, argv, MAX_ARGS);

    if (argc == 0) {
        vga_putc('\n');
        shell_print_prompt();
        memset(cmd_buffer, 0, CMD_BUFFER_SIZE);
        cmd_pos = 0;
        return;
    }

    // Find and execute command
    int found = 0;
    for (int i = 0; commands[i].name; i++) {
        if (strcmp(argv[0], commands[i].name) == 0) {
            commands[i].handler(argc, argv);
            found = 1;
            break;
        }
    }

    if (!found) {
        vga_puts("\n");
        vga_set_color((vga_color_attr_t){VGA_COLOR_RED, VGA_COLOR_BLACK});
        vga_puts("Error: ");
        vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});
        vga_puts("Unknown command '");
        vga_puts(argv[0]);
        vga_puts("'\n");
        vga_puts("Type 'help' for available commands.\n\n");
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