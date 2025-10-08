#include "vfs.h"
#include "../libc/stddef.h"
#include "../mm/memory.h"
#include "../io/vga.h"

static file_t* root = NULL;

void vfs_init() {
    // Create root directory
    root = (file_t*) kmalloc(sizeof(file_t));
    strcpy(root->name, "/");
    root->type = FILE_TYPE_DIRECTORY;
    root->data = NULL;
    root->size = 0;
    root->next = NULL;
    root->first_child = NULL;
    root->parent = NULL;
}

const char* vfs_basename(const char* path){
    const char* last_slash = path;
    for(const char* p = path; *p; p++){
        if(*p == '/'){
            last_slash = p + 1;
        }
    }
    return last_slash;
}

char* vfs_dirname(const char* path){
    static char dir[MAX_PATH];
    size_t len = strlen(path);

    //Find last slash
    size_t last_slash = 0;
    for(size_t i = 0; i < len; i++){
        if(path[i] == '/'){
            last_slash = i;
        }
    }

    if(last_slash == 0){
        strcpy(dir, "/");
    }else{
        strncpy(dir, path, last_slash);
        dir[last_slash] = '\0';
    }

    return dir;
}

//Resolve a path to a file/directory
file_t* vfs_resolve_path(const char* path){
    if(!root) return NULL;

    if(strcmp(path, "/") == 0){
        return root;
    }

    file_t* current = root;

    char path_copy[MAX_PATH];
    strcpy(path_copy, path);

    char* component = path_copy;
    if(*component == '/') component++;

    char* next_slash;
    while(*component){
        //Find next slash or string
        next_slash = component;
        while(*next_slash && *next_slash != '/') {
            next_slash++;
        }

        char saved = *next_slash;
        *next_slash = '\0';

        file_t* child = current->first_child;
        int found = 0;

        while(child){
            if(strcmp(child->name, component) == 0) {
                current = child;
                found = 1;
                break;
            }
            child = child->next;
        }

        if(!found){
            return NULL;
        }

        *next_slash = saved;
        if(*next_slash ==  '/'){
            component = next_slash + 1;
        }else{
            break;
        }
    }
    return current;
}

file_t* vfs_create_file(const char* path){
    char* dir_path = vfs_dirname(path);
    file_t* parent = vfs_resolve_path(dir_path);

    if(!parent || parent->type != FILE_TYPE_DIRECTORY){
        return NULL;
    }

    const char* filename = vfs_basename(path);

    file_t* child = parent->first_child;
    while(child){
        if(strcmp(child->name, filename) == 0){
            return child;
        }
        child = child->next;
    }

    file_t* file = (file_t*)kmalloc(sizeof(file_t));
    strncpy(file->name, filename, MAX_FILENAME -1);
    file->name[MAX_FILENAME - 1] = '\0';
    file->type = FILE_TYPE_REGULAR;
    file->data = NULL;
    file->size = 0;
    file->next = parent->first_child;
    file->first_child = NULL;
    file->parent = parent;

    parent->first_child = file;

    return file;
}

file_t* vfs_create_directory(const char* path){
    char* dir_path = vfs_dirname(path);
    file_t* parent = vfs_resolve_path(dir_path);

    if(!parent || parent->type != FILE_TYPE_DIRECTORY){
        return NULL;
    }

    const char* dirname = vfs_basename(path);

    file_t* child = parent->first_child;
    while(child){
        if(strcmp(child->name, dirname) == 0){
            return child;
        }
        child = child->next;
    }

    file_t* dir = (file_t*)kmalloc(sizeof(file_t));
    strncpy(dir->name, dirname, MAX_FILENAME -1);
    dir->name[MAX_FILENAME - 1] = '\0';
    dir->type = FILE_TYPE_DIRECTORY;
    dir->data = NULL;
    dir->size = 0;
    dir->next = parent->first_child;
    dir->first_child = NULL;
    dir->parent = parent;

    parent->first_child = dir;

    return dir;
}

file_t* vfs_open(const char* path){
    return vfs_resolve_path(path);
}

int vfs_write(file_t* file, const void* data, size_t size){
    if(!file || file->type != FILE_TYPE_REGULAR){
        return -1;
    }

    if(file->data){
        kfree(file->data);
    }

    file->data = (uint8_t*)kmalloc(size);
    if(!file->data){
        file->size = 0;
        return -1;
    }

    uint8_t* src = (uint8_t*)data;
    for(size_t i = 0; i < size; i++){
        file->data[i] = src[i];
    }

    file->size = size;

    return size;
}

int vfs_read(file_t* file, void* buffer, size_t size){
    if(!file || file->type != FILE_TYPE_REGULAR){
        return -1;
    }

    size_t to_read = (size < file->size) ? size : file->size;

    uint8_t* dst = (uint8_t*)buffer;
    for(size_t i = 0; i < to_read; i++){
        dst[i] = file->data[i];
    }

    return to_read;
}

int vfs_delete(const char* path){
    file_t* file = vfs_resolve_path(path);
    if(!file || file == root){
        return -1;
    }

    file_t* parent = file->parent;

    if(parent->first_child == file){
        parent->first_child = file->next;
    }else{
        file_t* prev = parent->first_child;
        while(prev && prev->next != file){
            prev = prev->next;
        }
        if(prev){
            prev->next = file->next;
        }
    }


    if(file->data){
        kfree(file->data);
    }

    kfree(file);

    return 0;
}

int vfs_copy_file(const char* dest_path, file_t* source, size_t size){
    if(strcmp(dest_path, "/")){
        vga_perror("Cannot overwrite root\n");
        return -1;
    }

    file_t* dest_file = vfs_resolve_path(dest_path);
    if(!dest_file){
        dest_file = vfs_create_file(dest_path);
    }

    for(size_t i = 0; i < size; i++){
        dest_file->data[i] = source->data[i];
    }

    return 0;
}

void vfs_list(const char* path){
    file_t* dir = vfs_resolve_path(path);

    if(!dir){
        vga_perror("Directory not found\n");
        return;
    }

    if(dir->type != FILE_TYPE_DIRECTORY){
        vga_perror("Not a directory\n");
        return;
    }

    file_t* child = dir->first_child;
    if(!child){
        vga_perror("Directory empty\n");
        return;
    }

    while(child){
        vga_set_color((vga_color_attr_t){
                child->type == FILE_TYPE_DIRECTORY ? VGA_COLOR_LIGHT_BLUE : VGA_COLOR_WHITE,
                VGA_COLOR_BLACK
        });

        vga_puts(child->name);
        if(child->type == FILE_TYPE_DIRECTORY){
            vga_putc('/');
        }else{
            vga_putc(' ');
            char size_str[32];
            uitoa(child->size, size_str);
            vga_puts(size_str);
            vga_puts(" bytes");
        }
        vga_putc('\n');

        child = child->next;
    }
    vga_set_color((vga_color_attr_t) {VGA_COLOR_WHITE,VGA_COLOR_BLACK});
}

void vfs_print_tree(file_t* dir, int depth) {
    if (!dir) dir = root;

    for (int i = 0; i < depth; i++) {
        vga_puts("  ");
    }

    vga_set_color((vga_color_attr_t){
            dir->type == FILE_TYPE_DIRECTORY ? VGA_COLOR_LIGHT_BLUE : VGA_COLOR_WHITE,
            VGA_COLOR_BLACK
    });

    vga_puts(dir->name);
    if (dir->type == FILE_TYPE_DIRECTORY) {
        vga_puts("/");
    }
    vga_puts("\n");

    vga_set_color((vga_color_attr_t){VGA_COLOR_WHITE, VGA_COLOR_BLACK});

    if (dir->type == FILE_TYPE_DIRECTORY) {
        file_t* child = dir->first_child;
        while (child) {
            vfs_print_tree(child, depth + 1);
            child = child->next;
        }
    }
}