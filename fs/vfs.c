#include "vfs.h"
#include "libc/stddef.h"
#include "console/console.h"

//Mount table
static mount_point_t mount_table[MAX_MOUNTS];
static vfs_node_t* vfs_root = NULL;

kerr_t vfs_init(){
    //Clear mount table
    for(int i = 0; i < MAX_MOUNTS; i++) {
        mount_table[i].in_use = 0;
        mount_table[i].fs = NULL;
    }
    vfs_root = NULL;
    return E_OK;
}

kerr_t vfs_mount(filesystem_t* fs, const char* path){
    if(!fs) return E_INVALID;

    //Find free mount point
    int slot = -1;
    for(int i = 0; i < MAX_MOUNTS; i++){
        if(!mount_table[i].in_use){
            slot = i;
            break;
        }
    }

    if(slot == -1) return E_NOMEM;

    //Call the filesystems mount function
    if(!fs->mount) return E_INVALID;
    kerr_t err = fs->mount(fs, NULL);
    if(err != E_OK) return err;

    //Add to mount table
    strcpy(mount_table[slot].path, path);
    mount_table[slot].fs = fs;
    mount_table[slot].in_use = 1;

    //If mounting at root, set vfs_root
    if(strcmp(path, "/") == 0){
        vfs_root = fs->root;
    }

    return E_OK;
}

kerr_t vfs_unmount(const char* path){
    for(int i = 0; i < MAX_MOUNTS; i++){
        if(mount_table[i].in_use && strcmp(path, mount_table[i].path) == 0){
            filesystem_t* fs = mount_table[i].fs;

            if(!fs->unmount) return E_NOTFOUND;
            kerr_t err = fs->unmount(fs);
            if(err != E_OK) return err;

            mount_table[i].in_use = 0;
            mount_table[i].fs = NULL;

            return E_OK;
        }
    }
    return E_NOTFOUND;
}

static filesystem_t* vfs_get_fs_for_path(const char* path){
    for (int i = 0; i < MAX_MOUNTS; i++) {
        if (mount_table[i].in_use) {
            size_t mount_len = strlen(mount_table[i].path);
            if (strncmp(path, mount_table[i].path, mount_len) == 0) {
                return mount_table[i].fs;
            }
        }
    }
    return NULL;
}

vfs_node_t* vfs_resolve_path(const char* path){
    if(!vfs_root || !path) return NULL;

    if(strcmp(path, "/") == 0) return vfs_root;

    // Start from root and traverse
    vfs_node_t* current = vfs_root;

    char path_copy[MAX_PATH];
    strcpy(path_copy, path);

    char* component = path_copy;
    if(*component == '/') component++;

    char* next_slash;
    while(*component){
        next_slash = component;
        while(*next_slash && *next_slash != '/'){
            next_slash++;
        }

        char saved = *next_slash;
        *next_slash = '\0';

        //Find the child with that name
        if (current->ops && current->ops->readdir) {
            int found = 0;
            uint32_t index = 0;
            vfs_node_t* child = NULL;

            while (current->ops->readdir(current, index, &child) == E_OK) {
                if (child && strcmp(child->name, component) == 0) {
                    current = child;
                    found = 1;
                    break;
                }
                index++;
            }

            if(!found) return NULL;
        }else{
            return NULL;
        }

        *next_slash = saved;
        if (*next_slash == '/') {
            component = next_slash + 1;
        } else {
            break;
        }
    }

    return current;
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

    size_t last_slash = 0;
    for(size_t i; i < len; i++){
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

vfs_node_t* vfs_open(const char* path) {
    return vfs_resolve_path(path);
}

kerr_t vfs_close(vfs_node_t* node) {
    if (!node || !node->ops || !node->ops->close) return E_OK;
    return node->ops->close(node);
}

kerr_t vfs_read(vfs_node_t* node, void* buffer, size_t size, size_t* bytes_read) {
    if (!node || !node->ops || !node->ops->read) return E_INVALID;
    return node->ops->read(node, buffer, size, bytes_read);
}

kerr_t vfs_write(vfs_node_t* node, const void* buffer, size_t size, size_t* bytes_written) {
    if (!node || !node->ops || !node->ops->write) return E_INVALID;
    return node->ops->write(node, buffer, size, bytes_written);
}

kerr_t vfs_create_file(const char* path) {
    char* dir_path = vfs_dirname(path);
    vfs_node_t* parent = vfs_resolve_path(dir_path);

    if (!parent /*|| !parent->ops || !parent->ops->create*/) {
        return E_INVALID;
    }else if(!parent->ops) return E_NOTFOUND;
    else if(!parent->ops->create) return E_NOTDIR;

    const char* filename = vfs_basename(path);
    vfs_node_t* new_file = NULL;

    return parent->ops->create(parent, filename, FILE_TYPE_REGULAR, &new_file);
}

kerr_t vfs_create_directory(const char* path) {
    char* dir_path = vfs_dirname(path);
    vfs_node_t* parent = vfs_resolve_path(dir_path);

    if (!parent || !parent->ops || !parent->ops->create) return E_INVALID;

    const char* dirname = vfs_basename(path);
    vfs_node_t* new_dir = NULL;

    return parent->ops->create(parent, dirname, FILE_TYPE_DIRECTORY, &new_dir);
}

kerr_t vfs_delete(const char* path) {
    vfs_node_t* node = vfs_resolve_path(path);
    if (!node || !node->ops || !node->ops->delete) return E_INVALID;
    return node->ops->delete(node);
}

kerr_t vfs_list(const char* path) {
    vfs_node_t* dir = vfs_resolve_path(path);

    if (!dir) return E_NOTFOUND;
    if (dir->type != FILE_TYPE_DIRECTORY) return E_NOTDIR;
    if (!dir->ops || !dir->ops->readdir) return E_INVALID;

    uint32_t index = 0;
    vfs_node_t* child = NULL;

    while (dir->ops->readdir(dir, index, &child) == E_OK) {
        if (child) {
            console_set_color((console_color_attr_t){
                    child->type == FILE_TYPE_DIRECTORY ? CONSOLE_COLOR_LIGHT_BLUE : CONSOLE_COLOR_WHITE,
                    CONSOLE_COLOR_BLACK
            });

            console_puts(child->name);
            if (child->type == FILE_TYPE_DIRECTORY) {
                console_putc('/');
            } else {
                console_putc(' ');
                char size_str[32];
                uitoa(child->size, size_str);
                console_puts(size_str);
                console_puts(" bytes");
            }
            console_putc('\n');
        }
        index++;
    }

    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});
    return E_OK;
}

void vfs_print_tree(vfs_node_t* node, int depth) {
    if (!node) node = vfs_root;
    if (!node) return;

    for (int i = 0; i < depth; i++) {
        console_puts("  ");
    }

    console_set_color((console_color_attr_t){
            node->type == FILE_TYPE_DIRECTORY ? CONSOLE_COLOR_LIGHT_BLUE : CONSOLE_COLOR_WHITE,
            CONSOLE_COLOR_BLACK
    });

    console_puts(node->name);
    if (node->type == FILE_TYPE_DIRECTORY) {
        console_puts("/");
    }
    console_puts("\n");

    console_set_color((console_color_attr_t){CONSOLE_COLOR_WHITE, CONSOLE_COLOR_BLACK});

    if (node->type == FILE_TYPE_DIRECTORY && node->ops && node->ops->readdir) {
        uint32_t index = 0;
        vfs_node_t* child = NULL;

        while (node->ops->readdir(node, index, &child) == E_OK) {
            if (child) {
                vfs_print_tree(child, depth + 1);
            }
            index++;
        }
    }
}

kerr_t vfs_copy_file(const char* dest, const char* source) {
    vfs_node_t* src = vfs_open(source);
    if (!src || src->type != FILE_TYPE_REGULAR) return E_INVALID;

    // Create destination file
    kerr_t err = vfs_create_file(dest);
    if (err != E_OK && err != E_EXISTS) return err;

    vfs_node_t* dst = vfs_open(dest);
    if (!dst) return E_NOTFOUND;

    // Read source
    void* buffer = (void*)src->fs_data; // Direct access for now
    size_t bytes_written = 0;

    return vfs_write(dst, buffer, src->size, &bytes_written);
}