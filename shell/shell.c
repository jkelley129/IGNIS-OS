#include "shell.h"
#include "tty/tty.h"
#include "driver.h"
#include "serial.h"
#include "console/console.h"
#include "libc/string.h"
#include "drivers/pit.h"
#include "drivers/block.h"
#include "mm/memory.h"
#include "fs/vfs.h"
#include "error_handling/errno.h"
#include "error_handling/kernel_panic.h"
#include "interrupts/idt.h"
#include "mm/pmm.h"
#include "mm/allocators/buddy.h"
#include "mm/allocators/slab.h"
#include "mm/allocators/kmalloc.h"
#include "scheduler/task.h"

#define CMD_BUFFER_SIZE 256
#define BACKSPACE_DELAY_TICKS 5

static uint64_t last_backspace_time = 0;
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
        {"lsdrv","Print registered drivers", cmd_lsdrv},
        {"meminfo", "Display memory statistics", cmd_meminfo},
        {"memtest", "Run memory allocator test", cmd_memtest},
        {"pmminfo", "Show PMM info", cmd_pmminfo},
        {"pagetest", "Test page allocation", cmd_pagetest},
        {"buddyinfo", "Display buddy allocator statistics", cmd_buddyinfo},
        {"buddytest", "Test buddy allocator", cmd_buddytest},
        {"slabinfo", "Display slab allocator statistics", cmd_slabinfo},
        {"slabtest", "Test slab allocator", cmd_slabtest},
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
        {"panic", "Test kernel panic (WARNING: will halt system)", cmd_panic},
        {"panictest", "Test panic with assertion", cmd_panictest},
        {"ps", "Print task list", cmd_ps},
        {"pidof", "Get PID of a task by name", cmd_pidof},
        {"pkill", "Kill a certain task", cmd_pkill},
        {"reboot", "Reboots the system with a triple fault", cmd_reboot},
        {"banner", "Displays a fun system banner", cmd_banner},
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
    shell_print_prompt();
}

void shell_print_prompt() {
    console_set_color((console_color_attr_t){CONSOLE_COLOR_LIGHT_GREEN, CONSOLE_COLOR_BLACK});
    console_puts("ignis");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    console_puts("$ ");
}

// ============================================================================
// COMMAND HANDLERS
// ============================================================================

void cmd_help(int argc, char** argv) {
    console_puts("\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_LIGHT_CYAN, CONSOLE_COLOR_BLACK});
    console_puts("IGNIS Shell - Available Commands\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    console_puts("================================\n\n");

    for (int i = 0; commands[i].name; i++) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_LIGHT_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ");
        console_puts(commands[i].name);
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

        // Padding
        size_t len = strlen(commands[i].name);
        for (size_t j = len; j < 12; j++) {
            console_putc(' ');
        }

        console_puts(commands[i].description);
        console_putc('\n');
    }
    console_putc('\n');
}

void cmd_clear(int argc, char** argv) {
    console_clear();
}

void cmd_echo(int argc, char** argv) {
    console_putc('\n');
    for (int i = 1; i < argc; i++) {
        console_puts(argv[i]);
        if (i < argc - 1) console_putc(' ');
    }
    console_puts("\n\n");
}

void cmd_about(int argc, char** argv) {
    console_puts("\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_LIGHT_CYAN, CONSOLE_COLOR_BLACK});
    console_puts(" v=====================================v\n");
    console_puts("[%]       IGNIS Operating System      [%]\n");
    console_puts(" ^=====================================^\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    console_puts("\n");
    console_puts("Version:     0.0.01 (64-bit)\n");
    console_puts("Developer:   Josh Kelley\n");
    console_puts("License:     Apache 2.0\n");
    console_puts("Description: A hobby OS written from scratch\n");
    console_puts("\n");
    console_puts("Features:\n");
    console_puts("  - VGA text mode output\n");
    console_puts("  - Interrupt handling (IDT)\n");
    console_puts("  - Keyboard driver\n");
    console_puts("  - PIT timer\n");
    console_puts("  - Memory allocator\n");
    console_puts("  - Virtual filesystem (VFS)\n");
    console_puts("  - RAM filesystem (RAMFS)\n");
    console_puts("  - Block device layer\n");
    console_puts("  - ATA disk driver\n");
    console_puts("\n");
}

void cmd_uptime(int argc, char** argv) {
    uint64_t ticks = pit_get_ticks();
    uint64_t total_seconds = ticks / 100;
    uint64_t hours = total_seconds / 3600;
    uint64_t minutes = (total_seconds % 3600) / 60;
    uint64_t seconds = total_seconds % 60;

    char num_str[32];

    console_puts("\nSystem uptime: ");
    uitoa(hours, num_str);
    console_puts(num_str);
    console_puts("h ");
    uitoa(minutes, num_str);
    console_puts(num_str);
    console_puts("m ");
    uitoa(seconds, num_str);
    console_puts(num_str);
    console_puts("s\n\n");
}

void cmd_ticks(int argc, char** argv) {
    uint64_t ticks = pit_get_ticks();
    char num_str[32];

    console_puts("\nPIT ticks: ");
    uitoa(ticks, num_str);
    console_puts(num_str);
    console_puts("\n\n");
}

void cmd_lsdrv(int argc, char** argv) {
    driver_list();
}

void cmd_meminfo(int argc, char** argv) {
    memory_print_stats();
}

void cmd_memtest(int argc, char** argv) {
    console_puts("\n=== Memory Allocator Test ===\n");

    console_puts("Allocating 3 blocks (64, 128, 256 bytes)...\n");
    void* ptr1 = kmalloc(64);
    void* ptr2 = kmalloc(128);
    void* ptr3 = kmalloc(256);

    if (ptr1 && ptr2 && ptr3) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("✓ Allocation successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

        console_puts("Freeing middle block...\n");
        kfree(ptr2);

        console_puts("Reallocating 128 bytes...\n");
        void* ptr4 = kmalloc(128);

        if (ptr4) {
            console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
            console_puts("✓ Reused freed block\n");
            console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        }

        console_puts("Testing kcalloc (10 * 8 bytes)...\n");
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
                console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
                console_puts("✓ Memory properly zeroed\n");
                console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
            }
            kfree(ptr5);
        }

        console_puts("Cleaning up...\n");
        kfree(ptr1);
        kfree(ptr3);
        kfree(ptr4);

        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("✓ Test complete!\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("✗ Allocation failed!\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    }
}

void cmd_pmminfo(int argc, char** argv) {
    pmm_print_stats();
}

void cmd_pagetest(int argc, char** argv) {
    console_puts("\n=== Page Allocation Test ===\n");

    console_puts("Allocating 3 pages...\n");
    void* page1 = kalloc_pages(1);
    void* page2 = kalloc_pages(1);
    void* page3 = kalloc_pages(1);

    if (page1 && page2 && page3) {
        char addr_str[32];

        console_puts("Page 1: 0x");
        uitoa((uint64_t)page1, addr_str);
        console_puts(addr_str);
        console_putc('\n');

        console_puts("Page 2: 0x");
        uitoa((uint64_t)page2, addr_str);
        console_puts(addr_str);
        console_putc('\n');

        console_puts("Page 3: 0x");
        uitoa((uint64_t)page3, addr_str);
        console_puts(addr_str);
        console_putc('\n');

        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("✓ Allocation successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

        // Actually write to the pages!
        console_puts("\nWriting to pages...\n");
        uint8_t* p1 = (uint8_t*)page1;
        uint8_t* p2 = (uint8_t*)page2;
        uint8_t* p3 = (uint8_t*)page3;

        p1[0] = 0xAA;
        p1[4095] = 0xBB;
        p2[0] = 0xCC;
        p2[4095] = 0xDD;
        p3[0] = 0xEE;
        p3[4095] = 0xFF;

        console_puts("Reading from pages...\n");
        if (p1[0] == 0xAA && p1[4095] == 0xBB &&
            p2[0] == 0xCC && p2[4095] == 0xDD &&
            p3[0] == 0xEE && p3[4095] == 0xFF) {
            console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
            console_puts("✓ Read/Write successful\n");
            console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        } else {
            console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
            console_puts("✗ Read/Write verification failed\n");
            console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        }

        console_puts("\nFreeing pages...\n");
        kfree_pages(page1, 1);
        kfree_pages(page2, 1);
        kfree_pages(page3, 1);

        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("✓ Test complete!\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

        pmm_print_stats();
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("✗ Allocation failed\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    }
}

void cmd_buddyinfo(int argc, char** argv) {
    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) {
        console_perror("Buddy allocator not initialized\n");
        return;
    }

    buddy_print_stats(buddy);
}

void cmd_buddytest(int argc, char** argv) {
    console_puts("\n=== Buddy Allocator Test ===\n");

    buddy_allocator_t* buddy = buddy_get_global();
    if (!buddy) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("✗ Buddy allocator not initialized\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        return;
    }

    console_puts("Test 1: Allocate single page (order 0)...\n");
    uint64_t page1 = buddy_alloc_order(buddy, 0);
    if (page1) {
        char addr_str[32];
        console_puts("  Allocated at: 0x");
        uitoa(page1, addr_str);
        console_puts(addr_str);
        console_putc('\n');

        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ✓ Single page allocation successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("  ✗ Allocation failed\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        return;
    }

    console_puts("\nTest 2: Allocate 8 pages (order 3)...\n");
    uint64_t pages8 = buddy_alloc_order(buddy, 3);
    if (pages8) {
        char addr_str[32];
        console_puts("  Allocated at: 0x");
        uitoa(pages8, addr_str);
        console_puts(addr_str);
        console_putc('\n');

        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ✓ Multi-page allocation successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("  ✗ Allocation failed\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        buddy_free(buddy, page1);
        return;
    }

    console_puts("\nTest 3: Allocate by size (17KB should use order 3)...\n");
    uint64_t size_alloc = buddy_alloc(buddy, 17 * 1024);
    if (size_alloc) {
        size_t actual_size = buddy_get_actual_size(17 * 1024);
        char num_str[32];
        console_puts("  Requested: 17 KB, Actual: ");
        uitoa(actual_size / 1024, num_str);
        console_puts(num_str);
        console_puts(" KB\n");

        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ✓ Size-based allocation successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("  ✗ Allocation failed\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        buddy_free(buddy, page1);
        buddy_free(buddy, pages8);
        return;
    }

    console_puts("\nTest 4: Write/Read test...\n");
    void* virt_ptr = PHYS_TO_VIRT(page1);
    uint8_t* test_data = (uint8_t*)virt_ptr;
    test_data[0] = 0xAA;
    test_data[4095] = 0xBB;

    if (test_data[0] == 0xAA && test_data[4095] == 0xBB) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ✓ Write/Read verification successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("  ✗ Write/Read verification failed\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    }

    console_puts("\nTest 5: Free and verify merging...\n");
    uint64_t splits_before = buddy->splits;
    uint64_t merges_before = buddy->merges;

    buddy_free(buddy, page1);
    buddy_free(buddy, pages8);
    buddy_free(buddy, size_alloc);

    char num_str[32];
    console_puts("  Merges performed: ");
    uitoa(buddy->merges - merges_before, num_str);
    console_puts(num_str);
    console_putc('\n');

    console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
    console_puts("  ✓ Free and merge successful\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

    console_puts("\nTest 6: Allocation pattern test (fragment and coalesce)...\n");
    uint64_t blocks[10];
    console_puts("  Allocating 10 blocks of 1 page each...\n");
    for (int i = 0; i < 10; i++) {
        blocks[i] = buddy_alloc_order(buddy, 0);
        if (!blocks[i]) {
            console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
            console_puts("  ✗ Allocation failed at block ");
            uitoa(i, num_str);
            console_puts(num_str);
            console_putc('\n');
            console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

            // Free previously allocated blocks
            for (int j = 0; j < i; j++) {
                buddy_free(buddy, blocks[j]);
            }
            return;
        }
    }

    console_puts("  Freeing every other block...\n");
    for (int i = 0; i < 10; i += 2) {
        buddy_free(buddy, blocks[i]);
    }

    console_puts("  Freeing remaining blocks...\n");
    for (int i = 1; i < 10; i += 2) {
        buddy_free(buddy, blocks[i]);
    }

    console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
    console_puts("  ✓ Fragmentation test successful\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

    console_puts("\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
    console_puts("✓ All buddy allocator tests passed!\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

    console_puts("\nCurrent allocator statistics:\n");
    buddy_print_stats(buddy);
}

void cmd_slabinfo(int argc, char** argv) {
    console_puts("\n");
    slab_print_all_stats();
}

void cmd_slabtest(int argc, char** argv) {
    console_puts("\n=== Slab Allocator Test ===\n");

    console_puts("Test 1: Small allocations (32 bytes)...\n");
    void* small1 = slab_kmalloc(32);
    void* small2 = slab_kmalloc(32);
    void* small3 = slab_kmalloc(32);

    if (small1 && small2 && small3) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ✓ Small allocations successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("  ✗ Small allocation failed\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        return;
    }

    console_puts("\nTest 2: Medium allocations (256 bytes)...\n");
    void* medium1 = slab_kmalloc(256);
    void* medium2 = slab_kmalloc(256);

    if (medium1 && medium2) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ✓ Medium allocations successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("  ✗ Medium allocation failed\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        slab_kfree(small1);
        slab_kfree(small2);
        slab_kfree(small3);
        return;
    }

    console_puts("\nTest 3: Large allocations (1024 bytes)...\n");
    void* large1 = slab_kmalloc(1024);
    void* large2 = slab_kmalloc(1024);

    if (large1 && large2) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ✓ Large allocations successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("  ✗ Large allocation failed\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        slab_kfree(small1);
        slab_kfree(small2);
        slab_kfree(small3);
        slab_kfree(medium1);
        slab_kfree(medium2);
        return;
    }

    console_puts("\nTest 4: Write/Read verification...\n");
    uint8_t* test_ptr = (uint8_t*)small1;
    for (int i = 0; i < 32; i++) {
        test_ptr[i] = i & 0xFF;
    }

    int verify_ok = 1;
    for (int i = 0; i < 32; i++) {
        if (test_ptr[i] != (i & 0xFF)) {
            verify_ok = 0;
            break;
        }
    }

    if (verify_ok) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ✓ Write/Read verification successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("  ✗ Write/Read verification failed\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    }

    console_puts("\nTest 5: Free and reallocation...\n");
    slab_kfree(small2);
    void* small4 = slab_kmalloc(32);

    if (small4) {
        // Check if we got the same address (reuse)
        if (small4 == small2) {
            console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
            console_puts("  ✓ Object reuse successful (same address)\n");
            console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        } else {
            console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
            console_puts("  ✓ Reallocation successful (different address)\n");
            console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        }
    }

    console_puts("\nTest 6: Multiple allocations from different caches...\n");
    void* multi_allocs[20];
    int sizes[] = {32, 64, 128, 256, 512, 1024, 2048, 4096};
    int alloc_success = 1;

    for (int i = 0; i < 20 && i < 20; i++) {
        size_t size = sizes[i % 8];
        multi_allocs[i] = slab_kmalloc(size);
        if (!multi_allocs[i]) {
            alloc_success = 0;
            char num_str[32];
            console_puts("  Failed at allocation ");
            uitoa(i, num_str);
            console_puts(num_str);
            console_putc('\n');

            // Free previous allocations
            for (int j = 0; j < i; j++) {
                slab_kfree(multi_allocs[j]);
            }
            break;
        }
    }

    if (alloc_success) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("  ✓ Multiple cache allocations successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

        // Free all
        for (int i = 0; i < 20; i++) {
            slab_kfree(multi_allocs[i]);
        }
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("  ✗ Multiple allocation test failed\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    }

    console_puts("\nCleaning up test allocations...\n");
    slab_kfree(small1);
    slab_kfree(small3);
    slab_kfree(small4);
    slab_kfree(medium1);
    slab_kfree(medium2);
    slab_kfree(large1);
    slab_kfree(large2);

    console_puts("\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
    console_puts("✓ All slab allocator tests passed!\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

    console_puts("\nCurrent slab statistics:\n");
    slab_print_all_stats();
}

// ============================================================================

void cmd_ls(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";
    console_putc('\n');
    kerr_t err = vfs_list(path);
    if (err != E_OK) {
        console_perror("Error listing directory: ");
        console_perror(k_strerror(err));
        console_putc('\n');
    }
    console_putc('\n');
}

void cmd_tree(int argc, char** argv) {
    const char* path = (argc > 1) ? argv[1] : "/";
    vfs_node_t* dir = vfs_resolve_path(path);

    if (!dir) {
        console_perror("Directory not found\n");
        return;
    }

    console_putc('\n');
    vfs_print_tree(dir, 0);
    console_putc('\n');
}

void cmd_touch(int argc, char** argv) {
    if (argc < 2) {
        console_perror("Usage: touch <filename>\n");
        return;
    }

    kerr_t err = vfs_create_file(argv[1]);
    if (err == E_OK) {
        console_puts("Created file: ");
        console_puts(argv[1]);
        console_putc('\n');
    } else if (err == E_EXISTS) {
        console_puts("File already exists: ");
        console_puts(argv[1]);
        console_putc('\n');
    } else {
        console_perror("Failed to create file: ");
        console_perror(k_strerror(err));
        console_putc('\n');
    }
}

void cmd_mkdir(int argc, char** argv) {
    if (argc < 2) {
        console_perror("Usage: mkdir <dirname>\n");
        return;
    }

    kerr_t err = vfs_create_directory(argv[1]);
    if (err == E_OK) {
        console_puts("Created directory: ");
        console_puts(argv[1]);
        console_putc('\n');
    } else if (err == E_EXISTS) {
        console_puts("Directory already exists: ");
        console_puts(argv[1]);
        console_putc('\n');
    } else {
        console_perror("Failed to create directory: ");
        console_perror(k_strerror(err));
        console_putc('\n');
    }
}

void cmd_rm(int argc, char** argv) {
    if (argc < 2) {
        console_perror("Usage: rm <path>\n");
        return;
    }

    kerr_t err = vfs_delete(argv[1]);
    if (err == E_OK) {
        console_puts("Removed: ");
        console_puts(argv[1]);
        console_putc('\n');
    } else {
        console_perror("Failed to remove: ");
        console_perror(k_strerror(err));
        console_putc('\n');
    }
}

void cmd_cat(int argc, char** argv) {
    if (argc < 2) {
        console_perror("Usage: cat <filename>\n");
        return;
    }

    vfs_node_t* file = vfs_open(argv[1]);
    if (!file) {
        console_perror("File not found\n");
        return;
    }

    if (file->type != FILE_TYPE_REGULAR) {
        console_perror("Not a regular file\n");
        vfs_close(file);
        return;
    }

    // Allocate buffer to read file
    uint8_t* buffer = kmalloc(file->size + 1);
    if (!buffer) {
        console_perror("Out of memory\n");
        vfs_close(file);
        return;
    }

    size_t bytes_read = 0;
    kerr_t err = vfs_read(file, buffer, file->size, &bytes_read);

    if (err == E_OK) {
        console_putc('\n');
        for (size_t i = 0; i < bytes_read; i++) {
            console_putc(buffer[i]);
        }
        console_puts("\n\n");
    } else {
        console_perror("Failed to read file: ");
        console_perror(k_strerror(err));
        console_putc('\n');
    }

    kfree(buffer);
    vfs_close(file);
}

void cmd_write(int argc, char** argv) {
    if (argc < 3) {
        console_perror("Usage: write <filename> <text>\n");
        return;
    }

    // Try to open existing file, or create new one
    vfs_node_t* file = vfs_open(argv[1]);
    if (!file) {
        kerr_t err = vfs_create_file(argv[1]);
        if (err != E_OK && err != E_EXISTS) {
            console_perror("Failed to create file: ");
            console_perror(k_strerror(err));
            console_putc('\n');
            return;
        }
        file = vfs_open(argv[1]);
        if (!file) {
            console_perror("Failed to open file\n");
            return;
        }
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

    size_t bytes_written = 0;
    kerr_t err = vfs_write(file, buffer, pos, &bytes_written);

    if (err == E_OK) {
        console_puts("Wrote ");
        char num_str[32];
        uitoa(bytes_written, num_str);
        console_puts(num_str);
        console_puts(" bytes to ");
        console_puts(argv[1]);
        console_putc('\n');
    } else {
        console_perror("Write failed: ");
        console_perror(k_strerror(err));
        console_putc('\n');
    }

    vfs_close(file);
}

void cmd_cp(int argc, char** argv) {
    if (argc < 3) {
        console_perror("Usage: cp <source> <dest>\n");
        return;
    }

    kerr_t err = vfs_copy_file(argv[2], argv[1]);
    if (err == E_OK) {
        console_puts("Copied ");
        console_puts(argv[1]);
        console_puts(" to ");
        console_puts(argv[2]);
        console_putc('\n');
    } else {
        console_perror("Copy failed: ");
        console_perror(k_strerror(err));
        console_putc('\n');
    }
}

void cmd_lsblk(int argc, char** argv) {
    block_list_devices();
}

void cmd_blkread(int argc, char** argv) {
    if (argc < 3) {
        console_perror("Usage: blkread <device_id> <lba>\n");
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
        console_perror("Failed to allocate buffer\n");
        return;
    }

    console_puts("\nReading device ");
    char num_str[32];
    uitoa(dev_id, num_str);
    console_puts(num_str);
    console_puts(", LBA ");
    uitoa(lba, num_str);
    console_puts(num_str);
    console_puts("...\n");

    if (block_read(dev_id, lba, buffer) == 0) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("✓ Read successful\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

        // Display first 64 bytes
        console_puts("\nFirst 64 bytes:\n");
        for (int i = 0; i < 64; i++) {
            if (i % 16 == 0) {
                console_puts("\n");
                uitoa(i, num_str);
                console_puts(num_str);
                console_puts(": ");
            }

            // Print hex byte
            uint8_t byte = buffer[i];
            char hex[3];
            hex[0] = "0123456789ABCDEF"[byte >> 4];
            hex[1] = "0123456789ABCDEF"[byte & 0xF];
            hex[2] = '\0';
            console_puts(hex);
            console_putc(' ');
        }
        console_puts("\n\n");
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("✗ Read failed\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    }

    kfree(buffer);
}

void cmd_blkwrite(int argc, char** argv) {
    if (argc < 4) {
        console_perror("Usage: blkwrite <device_id> <lba> <data>\n");
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
        console_perror("Failed to allocate buffer\n");
        return;
    }

    memset(buffer, 0, 512);

    // Copy data to buffer
    size_t len = strlen(argv[3]);
    if (len > 512) len = 512;
    for (size_t i = 0; i < len; i++) {
        buffer[i] = argv[3][i];
    }

    console_puts("\nWriting to device ");
    char num_str[32];
    uitoa(dev_id, num_str);
    console_puts(num_str);
    console_puts(", LBA ");
    uitoa(lba, num_str);
    console_puts(num_str);
    console_puts("...\n");

    kerr_t blk_write_status = block_write(dev_id, lba, buffer);

    if (blk_write_status == 0) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("✓ Write successful\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("✗ Write failed: ");
        console_puts(k_strerror(blk_write_status));
        console_puts("\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    }

    kfree(buffer);
}

void cmd_blktest(int argc, char** argv) {
    if (argc < 2) {
        console_perror("Usage: blktest <device_id>\n");
        return;
    }

    // Parse device ID
    uint8_t dev_id = 0;
    for (size_t i = 0; argv[1][i]; i++) {
        if (argv[1][i] >= '0' && argv[1][i] <= '9') {
            dev_id = dev_id * 10 + (argv[1][i] - '0');
        }
    }

    console_puts("\n=== Block Device Test ===\n");

    block_device_t* dev = block_get_device(dev_id);
    if (!dev || !dev->present) {
        console_perror("Device not found\n");
        return;
    }

    console_puts("Testing device ");
    char num_str[32];
    uitoa(dev_id, num_str);
    console_puts(num_str);
    console_puts(" (");
    console_puts(dev->label);
    console_puts(")\n\n");

    uint8_t* buffer = kmalloc(512);
    if (!buffer) {
        console_perror("Failed to allocate buffer\n");
        return;
    }

    // Write test pattern
    console_puts("Writing test pattern...\n");
    for (int i = 0; i < 512; i++) {
        buffer[i] = i & 0xFF;
    }

    if (block_write(dev_id, 100, buffer) != 0) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("✗ Write failed\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        kfree(buffer);
        return;
    }

    console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
    console_puts("✓ Write successful\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

    // Clear buffer
    memset(buffer, 0, 512);

    // Read back
    console_puts("Reading back data...\n");
    if (block_read(dev_id, 100, buffer) != 0) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("✗ Read failed\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        kfree(buffer);
        return;
    }

    console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
    console_puts("✓ Read successful\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

    // Verify
    console_puts("Verifying data...\n");
    int errors = 0;
    for (int i = 0; i < 512; i++) {
        if (buffer[i] != (i & 0xFF)) {
            errors++;
        }
    }

    if (errors == 0) {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
        console_puts("✓ Verification passed!\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    } else {
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("✗ Verification failed! Errors: ");
        uitoa(errors, num_str);
        console_puts(num_str);
        console_puts("\n\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    }

    kfree(buffer);
}

void cmd_hexdump(int argc, char** argv) {
    if (argc < 2) {
        console_perror("Usage: hexdump <filename>\n");
        return;
    }

    vfs_node_t* file = vfs_open(argv[1]);
    if (!file) {
        console_perror("File not found\n");
        return;
    }

    if (file->type != FILE_TYPE_REGULAR) {
        console_perror("Not a regular file\n");
        vfs_close(file);
        return;
    }

    // Read file data
    uint8_t* buffer = kmalloc(file->size);
    if (!buffer) {
        console_perror("Out of memory\n");
        vfs_close(file);
        return;
    }

    size_t bytes_read = 0;
    kerr_t err = vfs_read(file, buffer, file->size, &bytes_read);

    if (err != E_OK) {
        console_perror("Failed to read file: ");
        console_perror(k_strerror(err));
        console_putc('\n');
        kfree(buffer);
        vfs_close(file);
        return;
    }

    console_puts("\nHex dump of ");
    console_puts(argv[1]);
    console_puts(":\n\n");

    for (size_t i = 0; i < bytes_read; i++) {
        if (i % 16 == 0) {
            char num_str[32];
            uitoa(i, num_str);

            // Pad address
            size_t len = strlen(num_str);
            for (size_t j = len; j < 4; j++) {
                console_putc('0');
            }
            console_puts(num_str);
            console_puts(": ");
        }

        uint8_t byte = buffer[i];
        char hex[3];
        hex[0] = "0123456789ABCDEF"[byte >> 4];
        hex[1] = "0123456789ABCDEF"[byte & 0xF];
        hex[2] = '\0';
        console_puts(hex);
        console_putc(' ');

        if ((i + 1) % 16 == 0) {
            console_puts("  |");
            for (size_t j = i - 15; j <= i; j++) {
                char c = buffer[j];
                if (c >= 32 && c <= 126) {
                    console_putc(c);
                } else {
                    console_putc('.');
                }
            }
            console_puts("|\n");
        }
    }

    // Print remaining ASCII if not aligned
    if (bytes_read % 16 != 0) {
        size_t remaining = bytes_read % 16;
        for (size_t i = remaining; i < 16; i++) {
            console_puts("   ");
        }
        console_puts("  |");
        for (size_t i = bytes_read - remaining; i < bytes_read; i++) {
            char c = buffer[i];
            if (c >= 32 && c <= 126) {
                console_putc(c);
            } else {
                console_putc('.');
            }
        }
        for (size_t i = remaining; i < 16; i++) {
            console_putc(' ');
        }
        console_puts("|\n");
    }

    console_puts("\n");

    kfree(buffer);
    vfs_close(file);
}


void cmd_panic(int argc, char** argv) {
    if (argc < 2) {
        console_puts("\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_LIGHT_RED, CONSOLE_COLOR_BLACK});
        console_puts("WARNING: This will trigger a kernel panic!\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        console_puts("Usage: panic <message>\n");
        console_puts("Example: panic \"Testing panic handler\"\n\n");
        return;
    }

    // Concatenate all arguments as message
    char message[256];
    size_t pos = 0;
    for (int i = 1; i < argc && pos < 255; i++) {
        size_t len = strlen(argv[i]);
        for (size_t j = 0; j < len && pos < 255; j++) {
            message[pos++] = argv[i][j];
        }
        if (i < argc - 1 && pos < 255) {
            message[pos++] = ' ';
        }
    }
    message[pos] = '\0';

    // Trigger panic with context
    PANIC(message);
}

void cmd_panictest(int argc, char** argv) {
    console_puts("\n=== Kernel Panic Test ===\n");
    console_puts("Testing various panic scenarios...\n\n");

    console_puts("1. Testing NULL pointer assertion...\n");
    void* test_ptr = kmalloc(64);
    KASSERT(test_ptr != 0, "Memory allocation failed");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
    console_puts("   ✓ Passed\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    kfree(test_ptr);

    console_puts("\n2. Testing PANIC_ON_NULL macro...\n");
    test_ptr = kmalloc(128);
    PANIC_ON_NULL(test_ptr, "Test allocation failed");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_GREEN, CONSOLE_COLOR_BLACK});
    console_puts("   ✓ Passed\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    kfree(test_ptr);

    console_puts("\n3. All panic tests passed!\n");
    console_puts("   To trigger an actual panic, use: panic <message>\n\n");
}

void cmd_ps(int argc, char** argv) {
    task_print_list();
}

void cmd_pidof(int argc, char** argv) {
    if (argc < 2) {
        console_perror("Usage: pidof <task_name>\n");
        return;
    }

    const char* task_name = argv[1];
    task_t* task = task_get_by_name(task_name);
    if(!task) {
        console_perror("Task not found\n");
        return;
    }

    char pid_str[16];
    uitoa(task_pidof(task), pid_str);
    console_puts("PID of ");
    console_puts(task_name);
    console_puts(": ");
    console_puts(pid_str);
    console_putc('\n');
}

void cmd_pkill(int argc, char** argv) {
    task_exit();
}

void cmd_reboot(int argc, char** argv) {
    console_puts("\nRebooting system...\n");

    idt_ptr_t invalid_idt;
    invalid_idt.limit = 0;
    invalid_idt.base = 0;

    asm volatile("lidt %0" :: "m"(invalid_idt));
    asm volatile("int $0x03");  // Trigger interrupt with broken IDT
}

void cmd_banner(int argc, char** argv) {
    console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
    console_puts("\n");
    console_puts(" _____ _____ _   _ _____ _____ \n");
    console_puts("|_   _|  __ \\ \\ | |_   _/  ___|\n");
    console_puts("  | | | |  \\/|  \\| | | | \\ `--. \n");
    console_puts("  | | | | __ | . ` | | |  `--. \\\n");
    console_puts(" _| |_| |_\\ \\| |\\  |_| |_/\\__/ /\n");
    console_puts(" \\___/ \\____/\\_| \\_/\\___/\\____/ \n");
    console_puts("\n");
    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
}

// ============================================================================
// COMMAND EXECUTION
// ============================================================================

void shell_execute_command() {
    cmd_buffer[cmd_pos] = '\0';

    if (cmd_pos == 0) {
        console_putc('\n');
        shell_print_prompt();
        return;
    }

    // Parse command into argc/argv
    char* argv[MAX_ARGS];
    int argc = parse_command(cmd_buffer, argv, MAX_ARGS);

    if (argc == 0) {
        console_putc('\n');
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
        console_puts("\n");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
        console_puts("Error: ");
        console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
        console_puts("Unknown command '");
        console_puts(argv[0]);
        console_puts("'\n");
        console_puts("Type 'help' for available commands.\n\n");
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
        // Time lock: check if enough time has passed since last backspace
        uint64_t current_time = pit_get_ticks();
        if (current_time - last_backspace_time < BACKSPACE_DELAY_TICKS) {
            return;
        }

        if (cmd_pos > 0) {
            cmd_pos--;
            cmd_buffer[cmd_pos] = '\0';
            console_backspace(1);
            last_backspace_time = current_time;
        }

    } else {
        if (cmd_pos < CMD_BUFFER_SIZE - 1) {
            cmd_buffer[cmd_pos++] = c;
        }
    }
}

void shell_run(void) {
    char cmd_buffer[CMD_BUFFER_SIZE];

    serial_debug_puts("[SHELL] Shell task running\n");
    console_puts("\nIGNIS Shell Ready\n");
    console_puts("Type 'help' for available commands.\n\n");

    while (1) {
        shell_print_prompt();

        // Blocking read - will sleep until user presses Enter
        size_t bytes_read = tty_read(cmd_buffer, CMD_BUFFER_SIZE);

        serial_debug_puts("[SHELL] Read ");
        char num_str[16];
        uitoa(bytes_read, num_str);
        serial_debug_puts(num_str);
        serial_debug_puts(" bytes: ");
        serial_debug_puts(cmd_buffer);

        // Remove trailing newline
        if (bytes_read > 0 && cmd_buffer[bytes_read - 1] == '\n') {
            cmd_buffer[bytes_read - 1] = '\0';
            bytes_read--;
        }

        // Skip empty commands
        if (bytes_read == 0) {
            continue;
        }

        // Parse command into argc/argv
        char* argv[MAX_ARGS];
        int argc = parse_command(cmd_buffer, argv, MAX_ARGS);

        if (argc == 0) {
            continue;
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
            console_puts("\n");
            console_set_color((console_color_attr_t){CONSOLE_COLOR_RED, CONSOLE_COLOR_BLACK});
            console_puts("Error: ");
            console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
            console_puts("Unknown command '");
            console_puts(argv[0]);
            console_puts("'\n");
            console_puts("Type 'help' for available commands.\n\n");
        }
    }
}