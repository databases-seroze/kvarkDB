#include "log.h"

#include <stdio.h>
#include <stdbool.h>
#include<stdlib.h>

typedef enum {
    LOG_ERROR,
    LOG_WARN,
    LOG_INFO,
    LOG_DEBUG,
} log_level_t;

int log_create(log_t** log,  const char* path, bool truncate) {
    *log = malloc(sizeof(log_t));
    (*log)->file = fopen(path, truncate ? "w": "a"); //write or append mode
    (*log)->debug_enabled = KVDB_DEBUB_LOG;
    return 0;
}
