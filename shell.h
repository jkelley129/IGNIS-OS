#ifndef SHELL_H
#define SHELL_H

void shell_init();
void shell_handle_char(char c);
void shell_print_prompt();
void shell_execute_command();

#endif