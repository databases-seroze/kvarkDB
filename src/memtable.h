#ifndef __KVARKDB_MEMTABLE_H__
#define __KVARKDB_MEMTABLE_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <time.h>
#include "skiplist.h"

typedef struct {
    skiplist_t* skiplist;
    size_t size_bytes;   // running total of key + value bytes, used to decide when to flush
    bool immutable;      // set to true when flush is triggered; no further writes allowed
} memtable_t;

// create a new memtable
int memtable_new(memtable_t** memtable, size_t max_levels, float probability);

// free the memtable and its skiplist
int memtable_destroy(memtable_t** memtable);

// insert or update a key-value pair
// returns -1 if the memtable is immutable
int memtable_put(memtable_t* memtable, const uint8_t* key, size_t key_size,
                 uint8_t* value, size_t value_size, time_t ttl);

// look up a key
int memtable_get(memtable_t* memtable, const uint8_t* key, size_t key_size,
                 uint8_t** value, size_t** value_size);

// delete a key
int memtable_delete(memtable_t* memtable, const uint8_t* key, size_t key_size);

// returns true if size_bytes >= max_size
bool memtable_needs_flush(memtable_t* memtable, size_t max_size);

#endif
