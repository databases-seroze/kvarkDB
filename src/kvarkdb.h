#ifndef KVARKDB
#define KVARKDB

#include<stdbool.h>
#include<stddef.h>

// kvarkdb/
// ├── MANIFEST
// ├── kvarkdb.conf
// ├── wal/
// │   ├── 000001.log
// │   └── 000002.log
// ├── sstables/
// │   ├── level0/
// │   │   ├── 000001.sst
// │   │   └── 000002.sst
// │   └── level1/
// │       └── 000005.sst
// └── meta/
//     └── column_families/
//         ├── default.cf
//         └── users.cf

typedef struct kvarkdb_config{
    size_t memtable_max_size; // threshold to flush to sstable
    size_t sstable_target_size; // optional, could be 2*memtable_size
    char* db_path;

} kvarkdb_config_t;

typedef struct kvarkdb_column_family{
    char* name;
    struct skiplist_t *memtable;
} kvarkdb_column_family_t;

typedef struct kvarkdb{
    kvarkdb_config_t config;
    kvarkdb_column_family_t* column_families;
    size_t column_family_count;
} kvarkdb_t;

// database operations
int kvarkdb_open(kvarkdb_t** db, const kvarkdb_config_t* config);
void kvarkdb_close(kvarkdb_t* kvarkdb);

// column family operations
int kvarddb_create_column_family(kvarkdb_t* db, const char* name);
int kvarddb_drop_column_family(kvarkdb_t* db, const char* name);

// key-value operations
int kvarkdb_put(kvarkdb_t* db, const char* column_family, const char* key, const char* value);
char* kvarkdb_get(kvarkdb_t* db, const char* column_family, const char* key);
int kvarkdb_delete(kvarkdb_t* db, const char* column_family, const char* key);

#endif
