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

/*
 * flags bits for skiplist_node_t.
 * Using a dedicated field (not a magic sentinel in the value bytes) means
 * any user value — including one that happens to look like a tombstone —
 * is stored and returned correctly.
 */
#define SKIPLIST_FLAG_DELETED 0x01  /* node is a tombstone — key was deleted */

typedef struct skiplist_node_t {
    uint8_t* key;
    size_t key_size;
    uint8_t* value;       /* NULL for tombstone nodes */
    size_t value_size;    /* 0 for tombstone nodes */
    time_t ttl;
    uint8_t flags;        /* bitfield: see SKIPLIST_FLAG_* above */
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

// insert or update a key-value pair; flags may be SKIPLIST_FLAG_DELETED for a tombstone
// tombstones have value=NULL and value_size=0
int skiplist_put(skiplist_t** skiplist, const uint8_t* key, size_t key_size, uint8_t* value, size_t value_size, time_t ttl, uint8_t flags);

// look up a key; sets *value and *value_size on success
// if flags is non-NULL it receives the node's flags (e.g. SKIPLIST_FLAG_DELETED)
// returns 0 if found (including tombstones), -1 if not found or expired
int skiplist_get(skiplist_t* skiplist, const uint8_t* key, size_t key_size, uint8_t** value, size_t** value_size, uint8_t* flags);

// delete a key; pass value=NULL to skip value check
int skiplist_delete(skiplist_t** skiplist, const uint8_t* key, size_t key_size, const uint8_t* value, size_t value_size);

// cursor — iterates over nodes in key order via the level-0 linked list
// pointers returned by skiplist_cursor_get are owned by the skiplist, do not free them

// create a cursor positioned at the first node; returns -1 if the list is empty
int skiplist_cursor_init(skiplist_t* skiplist, skiplist_cursor_t** cursor);

// advance to the next node; returns -1 when exhausted
int skiplist_cursor_next(skiplist_cursor_t* cursor);

// move to the previous node; returns -1 when at the start
int skiplist_cursor_prev(skiplist_cursor_t* cursor);

// read the current node's key, value, and flags (pointers into the node, no copy)
// flags may be NULL if the caller does not need it
int skiplist_cursor_get(skiplist_cursor_t* cursor, uint8_t** key, size_t* key_size, uint8_t** value, size_t* value_size, uint8_t* flags);

// free the cursor (does not free the skiplist)
void skiplist_cursor_destroy(skiplist_cursor_t* cursor);

#endif
