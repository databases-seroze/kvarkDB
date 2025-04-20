#ifndef __KVARKDB_ERR_H__
#define __KVARKDB_ERR_H__

typedef struct {
    int code; //error code
    char* message; //error message
} kvarkdb_err_t;

#endif
