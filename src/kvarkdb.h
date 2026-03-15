#ifndef __KVARKDB_H__
#define __KVARKDB_H__

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "memtable.h"

// kvarkdb/
// ├── data/
// │   ├── meta/
// │   │   ├── column_families   (one CF name per line)
// │   │   └── manifest          (one "<cf_name> <sst_path>" per line)
// │   └── sstables/
// │       └── <cf_name>/
// │           ├── 1.sst
// │           └── 2.sst

typedef struct kvarkdb_config {
    size_t memtable_max_size;    // flush threshold in bytes
    size_t sstable_target_size;  // optional, could be 2 * memtable_max_size
    char*  db_path;
} kvarkdb_config_t;

typedef struct kvarkdb_column_family {
    char*       name;
    memtable_t* memtable;
    size_t      memtable_max_size;  // copied from config; per-CF tunable later
    char**      sst_paths;          // heap array of SSTable paths, oldest → newest
    size_t      sst_count;
    uint64_t    next_sst_seq;       // incremented on each flush
} kvarkdb_column_family_t;

typedef struct kvarkdb {
    kvarkdb_config_t          config;
    kvarkdb_column_family_t*  column_families;
    size_t                    column_family_count;
} kvarkdb_t;

// database operations
// caller allocates db, fills db->config, then calls kvarkdb_open
int  kvarkdb_open(kvarkdb_t* db);
void kvarkdb_close(kvarkdb_t* db);

// column family operations
int kvarddb_create_column_family(kvarkdb_t* db, const char* name);
int kvarddb_drop_column_family(kvarkdb_t* db, const char* name);

// key-value operations — keys and values are null-terminated C strings
// kvarkdb_get returns a heap-allocated string — caller must free()
// returns NULL if the key is not found
int   kvarkdb_put(kvarkdb_t* db, const char* column_family, const char* key, const char* value);
char* kvarkdb_get(kvarkdb_t* db, const char* column_family, const char* key);
int   kvarkdb_delete(kvarkdb_t* db, const char* column_family, const char* key);

#endif
