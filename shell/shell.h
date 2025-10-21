#ifndef SHELL_H
#define SHELL_H

#include "libc/stdint.h"

#define MAX_ARGS 16
#define MAX_ARG_LEN 64

// Command structure
typedef struct {
    const char* name;
    const char* description;
    void (*handler)(int argc, char** argv);
} shell_command_t;

void shell_init();
void shell_handle_char(char c);
void shell_print_prompt();
void shell_execute_command();

// Command handlers
void cmd_help(int argc, char** argv);
void cmd_clear(int argc, char** argv);
void cmd_echo(int argc, char** argv);
void cmd_about(int argc, char** argv);
void cmd_uptime(int argc, char** argv);
void cmd_ticks(int argc, char** argv);
void cmd_lsdrv(int argc, char** argv);
void cmd_meminfo(int argc, char** argv);
void cmd_memtest(int argc, char** argv);
void cmd_pmminfo(int argc, char** argv);
void cmd_pagetest(int argc, char** argv);
void cmd_ls(int argc, char** argv);
void cmd_tree(int argc, char** argv);
void cmd_touch(int argc, char** argv);
void cmd_mkdir(int argc, char** argv);
void cmd_rm(int argc, char** argv);
void cmd_cat(int argc, char** argv);
void cmd_write(int argc, char** argv);
void cmd_cp(int argc, char** argv);
void cmd_lsblk(int argc, char** argv);
void cmd_blkread(int argc, char** argv);
void cmd_blkwrite(int argc, char** argv);
void cmd_blktest(int argc, char** argv);
void cmd_hexdump(int argc, char** argv);
void cmd_panic(int argc, char** argv);
void cmd_panictest(int argc, char** argv);

#endif