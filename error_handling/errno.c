#include "errno.h"

const char* k_strerror(kerr_t err){
    switch(err){
        case E_OK: return "Success";
        case E_NOMEM: return "Out of Memory";
        case E_INVALID: return "Invalid argument";
        case E_NOTFOUND: return "Not found";
        case E_EXISTS: return "Already exists";
        case E_NOTDIR: return "Not a directory";
        case E_ISDIR: return "Is a directory";
        case E_PERM: return "Permission denied";
    }
}

