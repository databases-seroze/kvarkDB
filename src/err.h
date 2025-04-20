#ifndef __KVARKDB_ERR__
#define __KVARKDB_ERR__

typedef struct {
    int code; //error code
    char* message; //error message
} kvarkdb_err_t;

#endif
