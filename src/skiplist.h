#ifndef __KVARKDB_SKIPLIST_H__
#define __KVARKDB_SKIPLIST_H__

// #include <cstdint>
#include <time.h>
#include <stdio.h>
#include <stdint.h> // for uint8_t

typedef struct {
    uint8_t* key; //key
    size_t key_size; //key size
    uint8_t* value; //value
    size_t value_size; //value size
    time_t ttl; //time to live
} skiplist_node_t;

typedef struct {
    size_t max_levels;
    float probability;
    skiplist_node_t* head;
    skiplist_node_t* tail; // for faster access of last node
    size_t size; // current no of nodes
} skiplist_t;

typedef struct {
    skiplist_node_t* current;
    skiplist_t* skiplist;
} skiplist_cursor_t;

// let's first come up with the interface methods
size_t get_random_level();

// function to create new skiplist
int skiplist_new(skiplist_t** skiplist, size_t max_levels, float probability);

// function to delete a skiplist
// 0 if it's successful
int skiplist_clear(skiplist_t** skiplist);

// function to insert into skiplist
int skiplist_put(skiplist_t** skiplist, const uint8_t* key, size_t key_size, uint8_t* value, size_t value_size, time_t ttl);

// function to search for key in skiplist
// 0 if it's successful -1 otherwise
int skiplist_get(skiplist_t* skiplist, const uint8_t* key, size_t key_size, uint8_t** value, size_t** value_size);

int skiplist_delete(skiplist_t** skiplist, const uint8_t* key, size_t key_size, const uint8_t* value, size_t value_size);

#endif
