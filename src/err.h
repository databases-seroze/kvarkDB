#ifndef KVARKDB_ERR
#define KVARKDB_ERR

typedef struct {
    int code; //error code
    char* message; //error message
} kvarkdb_err_t;

#endif
