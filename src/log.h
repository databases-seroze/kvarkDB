
#include <stdbool.h>
#include <stdio.h>

#define KVDB_DEBUB_LOG false // false

typedef struct  {
    FILE *file;
    bool debug_enabled;
} log_t;

int log_create(log_t** log, const char* path, bool truncate); //Create/configure logger
void log_write(log_t** log, int level, const char* format, ...); // thread-safe printf
void log_free(log_t** log); //clean up
