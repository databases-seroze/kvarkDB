#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "kvarkdb.h"
#include "repl.h"

#define DEFAULT_DB_PATH     "kvarkdb_data"
#define DEFAULT_MEMTABLE_SZ (4 * 1024 * 1024)   /* 4 MiB */

static void usage(const char* prog) {
    fprintf(stderr, "usage: %s [--db <path>] [--memtable-size <bytes>]\n", prog);
    fprintf(stderr, "  --db <path>              database directory (default: %s)\n", DEFAULT_DB_PATH);
    fprintf(stderr, "  --memtable-size <bytes>  flush threshold    (default: %d)\n", DEFAULT_MEMTABLE_SZ);
}

int main(int argc, char** argv) {
    const char* db_path      = DEFAULT_DB_PATH;
    size_t      memtable_sz  = DEFAULT_MEMTABLE_SZ;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--db") == 0 && i + 1 < argc) {
            db_path = argv[++i];
        } else if (strcmp(argv[i], "--memtable-size") == 0 && i + 1 < argc) {
            memtable_sz = (size_t)strtoull(argv[++i], NULL, 10);
        } else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "unknown option: %s\n", argv[i]);
            usage(argv[0]);
            return 1;
        }
    }

    kvarkdb_t db = {0};
    db.config.db_path           = (char*)db_path;
    db.config.memtable_max_size = memtable_sz;
    db.config.sstable_target_size = 2 * memtable_sz;

    if (kvarkdb_open(&db) != 0) {
        fprintf(stderr, "error: failed to open database at '%s'\n", db_path);
        return 1;
    }

    repl_run(&db);
    kvarkdb_close(&db);
    return 0;
}
