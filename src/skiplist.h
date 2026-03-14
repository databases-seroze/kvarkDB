#ifndef __KVARKDB_SKIPLIST_H__
#define __KVARKDB_SKIPLIST_H__

#include <time.h>
#include <stdio.h>
#include <stdint.h>
#include <stddef.h>

// a single level pointer in a node (next, prev, span)
typedef struct {
    struct skiplist_node_t* next;
    struct skiplist_node_t* prev;
    size_t span;
} skiplist_level_t;

typedef struct skiplist_node_t {
    uint8_t* key;
    size_t key_size;
    uint8_t* value;
    size_t value_size;
    time_t ttl;
    skiplist_level_t* levels; // array of levels, length = node_level at creation
} skiplist_node_t;

typedef struct {
    size_t max_levels;
    float probability;
    skiplist_node_t* head;
    skiplist_node_t* tail; // for faster access of last node
    size_t size;
} skiplist_t;

typedef struct {
    skiplist_node_t* current;
    skiplist_t* skiplist;
} skiplist_cursor_t;

// returns a random level for a new node
size_t get_random_level(skiplist_t* skiplist);

// create a new skiplist
int skiplist_new(skiplist_t** skiplist, size_t max_levels, float probability);

// free all nodes and the skiplist itself; sets *skiplist to NULL
int skiplist_clear(skiplist_t** skiplist);

// insert or update a key-value pair
int skiplist_put(skiplist_t** skiplist, const uint8_t* key, size_t key_size, uint8_t* value, size_t value_size, time_t ttl);

// look up a key; sets *value and *value_size on success
// returns 0 on success, -1 if not found or expired
int skiplist_get(skiplist_t* skiplist, const uint8_t* key, size_t key_size, uint8_t** value, size_t** value_size);

// delete a key; pass value=NULL to skip value check
int skiplist_delete(skiplist_t** skiplist, const uint8_t* key, size_t key_size, const uint8_t* value, size_t value_size);

#endif
