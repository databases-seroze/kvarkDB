#include "memtable.h"
#include <stdlib.h>

#define MEMTABLE_MAX_LEVELS 12
#define MEMTABLE_PROBABILITY 0.5f

int memtable_new(memtable_t** memtable, size_t max_levels, float probability) {
    *memtable = (memtable_t*)malloc(sizeof(memtable_t));
    if (*memtable == NULL) return -1;

    if (skiplist_new(&(*memtable)->skiplist, max_levels, probability) != 0) {
        free(*memtable);
        *memtable = NULL;
        return -1;
    }

    (*memtable)->size_bytes = 0;
    (*memtable)->immutable  = false;
    return 0;
}

int memtable_destroy(memtable_t** memtable) {
    if (*memtable == NULL) return -1;

    skiplist_clear(&(*memtable)->skiplist);
    free(*memtable);
    *memtable = NULL;
    return 0;
}

int memtable_put(memtable_t* memtable, const uint8_t* key, size_t key_size,
                 uint8_t* value, size_t value_size, time_t ttl) {
    if (memtable == NULL || memtable->immutable) return -1;

    // check if key already exists to adjust size_bytes correctly on update
    uint8_t* old_value = NULL;
    size_t* old_value_size_ptr = NULL;
    bool is_update = skiplist_get(memtable->skiplist, key, key_size,
                                  &old_value, &old_value_size_ptr, NULL) == 0;

    // snapshot before skiplist_put overwrites the node's value_size in place
    size_t old_value_size = is_update ? *old_value_size_ptr : 0;

    if (skiplist_put(&memtable->skiplist, key, key_size, value, value_size, ttl, 0) != 0) {
        return -1;
    }

    if (is_update) {
        memtable->size_bytes -= old_value_size;
        memtable->size_bytes += value_size;
    } else {
        memtable->size_bytes += key_size + value_size;
    }

    return 0;
}

int memtable_get(memtable_t* memtable, const uint8_t* key, size_t key_size,
                 uint8_t** value, size_t** value_size, uint8_t* flags) {
    if (memtable == NULL) return -1;
    return skiplist_get(memtable->skiplist, key, key_size, value, value_size, flags);
}

int memtable_delete(memtable_t* memtable, const uint8_t* key, size_t key_size) {
    if (memtable == NULL || memtable->immutable) return -1;

    /* Write a tombstone node (flags = SKIPLIST_FLAG_DELETED, value = NULL).
     * The node stays in the skiplist so it gets flushed to SSTable, ensuring
     * keys in older SSTables are correctly suppressed on the read path. */
    uint8_t* old_value = NULL;
    size_t* old_value_size_ptr = NULL;
    bool is_update = skiplist_get(memtable->skiplist, key, key_size,
                                  &old_value, &old_value_size_ptr, NULL) == 0;

    size_t old_value_size = is_update ? *old_value_size_ptr : 0;

    if (skiplist_put(&memtable->skiplist, key, key_size, NULL, 0, 0,
                     SKIPLIST_FLAG_DELETED) != 0) {
        return -1;
    }

    if (is_update) {
        /* tombstone replaces existing value — subtract old value size only */
        memtable->size_bytes -= old_value_size;
    } else {
        /* new tombstone entry for a key only in SSTable */
        memtable->size_bytes += key_size;
    }

    return 0;
}

bool memtable_needs_flush(memtable_t* memtable, size_t max_size) {
    if (memtable == NULL) return false;
    return memtable->size_bytes >= max_size;
}
