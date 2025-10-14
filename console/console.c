#include "console.h"

static console_driver_t* driver = 0;

kerr_t console_init(console_driver_t* console_driver){
    if(!console_driver) return E_INVALID;

    driver = console_driver;

    if(console_driver->init) return console_driver->init();

    return E_OK;
}

void console_clear(void){
    if(driver && driver->clear) driver->clear();
}


void console_putc(char c){
    if(driver && driver->putc) driver->putc(c);
}

void console_puts(const char* str){
    if(driver && driver->puts) driver->puts(str);
}

void console_puts_color(const char* str, console_color_attr_t color){
    if(!driver || !driver->set_color || !driver->get_color) return;

    console_color_attr_t tmp = driver->get_color();
    console_set_color(color);

    console_puts(str);

    console_set_color(tmp);
}

void console_set_color(console_color_attr_t color){
    if(!driver || !driver->set_color) return;

    driver->set_color(color);
}

console_color_attr_t console_get_color(void){
    if(!driver || !driver->set_color){
        return (console_color_attr_t){CONSOLE_COLOR_WHITE,CONSOLE_COLOR_BLACK};
    }

    return driver->get_color();
}

void console_backspace(int count){
    if(!driver || !driver->backspace) return;

    driver->backspace(count);
}

void console_perror(const char* error_str){
    if(!driver) return;

    console_puts_color(error_str, CONSOLE_COLOR_FAILURE);
}