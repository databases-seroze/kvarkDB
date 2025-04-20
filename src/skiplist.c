#include "skiplist.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

static int key_compare(const uint8_t* key1, size_t key1_size,
                      const uint8_t* key2, size_t key2_size) {
    if (key1_size != key2_size) {
        return key1_size < key2_size ? -1 : 1;
    }
    return memcmp(key1, key2, key1_size);
}

size_t get_random_level(skiplist_t* skiplist) {
    size_t level = 1;
    while (((float)rand() / RAND_MAX) < skiplist->probability &&
           level < skiplist->max_levels) {
        level++;
    }
    return level;
}

int skiplist_new(skiplist_t** skiplist, size_t max_levels, float probability) {
    if (max_levels <= 0 || probability <= 0 || probability >= 1) {
        return -1;
    }

    *skiplist = (skiplist_t*)malloc(sizeof(skiplist_t));
    if (*skiplist == NULL) {
        return -1;
    }

    (*skiplist)->max_levels = max_levels;
    (*skiplist)->probability = probability;
    (*skiplist)->size = 0;

    // Create head node with max levels
    (*skiplist)->head = (skiplist_node_t*)malloc(sizeof(skiplist_node_t));
    if ((*skiplist)->head == NULL) {
        free(*skiplist);
        return -1;
    }

    (*skiplist)->head->max_levels = (skiplist_level_t*)calloc(max_levels, sizeof(skiplist_level_t));
    if ((*skiplist)->head->levels == NULL) {
        free((*skiplist)->head);
        free(*skiplist);
        return -1;
    }

    (*skiplist)->head->key = NULL;
    (*skiplist)->head->key_size = 0;
    (*skiplist)->head->value = NULL;
    (*skiplist)->head->value_size = 0;
    (*skiplist)->head->ttl = 0;

    // Initialize all head levels to point to NULL
    for (size_t i = 0; i < max_levels; i++) {
        (*skiplist)->head->levels[i].next = NULL;
        (*skiplist)->head->levels[i].prev = NULL;
        (*skiplist)->head->levels[i].span = 0;
    }

    (*skiplist)->tail = NULL;

    return 0;
}

static void free_node(skiplist_node_t* node) {
    if (node == NULL) return;

    if (node->key) free(node->key);
    if (node->value) free(node->value);
    if (node->levels) free(node->levels);
    free(node);
}

int skiplist_clear(skiplist_t** skiplist) {
    if (*skiplist == NULL) return -1;

    skiplist_node_t* current = (*skiplist)->head->levels[0].next;
    while (current != NULL) {
        skiplist_node_t* next = current->levels[0].next;
        free_node(current);
        current = next;
    }

    free((*skiplist)->head->levels);
    free((*skiplist)->head);
    free(*skiplist);
    *skiplist = NULL;

    return 0;
}

int skiplist_put(skiplist_t** skiplist, const uint8_t* key, size_t key_size,
                uint8_t* value, size_t value_size, time_t ttl) {
    if (*skiplist == NULL || key == NULL || value == NULL) {
        return -1;
    }

    skiplist_node_t* update[(*skiplist)->max_levels];
    skiplist_node_t* current = (*skiplist)->head;

    // Find the insertion point and keep track of the path
    for (int i = (*skiplist)->max_levels - 1; i >= 0; i--) {
        while (current->levels[i].next != NULL &&
               key_compare(current->levels[i].next->key, current->levels[i].next->key_size,
                          key, key_size) < 0) {
            current = current->levels[i].next;
        }
        update[i] = current;
    }

    // Check if key already exists
    if (current->levels[0].next != NULL &&
        key_compare(current->levels[0].next->key, current->levels[0].next->key_size,
                   key, key_size) == 0) {
        // Key exists, update the value
        skiplist_node_t* existing = current->levels[0].next;

        // Free old value if size differs
        if (existing->value_size != value_size) {
            free(existing->value);
            existing->value = (uint8_t*)malloc(value_size);
            if (existing->value == NULL) {
                return -1;
            }
        }

        memcpy(existing->value, value, value_size);
        existing->value_size = value_size;
        existing->ttl = ttl;
        return 0;
    }

    // Create new node
    skiplist_node_t* new_node = (skiplist_node_t*)malloc(sizeof(skiplist_node_t));
    if (new_node == NULL) {
        return -1;
    }

    size_t node_level = get_random_level(*skiplist);
    new_node->levels = (skiplist_level_t*)calloc(node_level, sizeof(skiplist_level_t));
    if (new_node->levels == NULL) {
        free(new_node);
        return -1;
    }

    new_node->key = (uint8_t*)malloc(key_size);
    new_node->value = (uint8_t*)malloc(value_size);
    if (new_node->key == NULL || new_node->value == NULL) {
        if (new_node->key) free(new_node->key);
        if (new_node->value) free(new_node->value);
        free(new_node->levels);
        free(new_node);
        return -1;
    }

    memcpy(new_node->key, key, key_size);
    new_node->key_size = key_size;
    memcpy(new_node->value, value, value_size);
    new_node->value_size = value_size;
    new_node->ttl = ttl;

    // Insert the new node
    for (size_t i = 0; i < node_level; i++) {
        new_node->levels[i].next = update[i]->levels[i].next;
        update[i]->levels[i].next = new_node;

        // Update spans
        if (i == 0) {
            new_node->levels[i].prev = update[i];
            if (new_node->levels[i].next) {
                new_node->levels[i].next->levels[i].prev = new_node;
            }
        }
    }

    // Update tail if this is the last node
    if (new_node->levels[0].next == NULL) {
        (*skiplist)->tail = new_node;
    }

    (*skiplist)->size++;

    return 0;
}

int skiplist_get(skiplist_t* skiplist, const uint8_t* key, size_t key_size,
                uint8_t** value, size_t** value_size) {
    if (skiplist == NULL || key == NULL) {
        return -1;
    }

    skiplist_node_t* current = skiplist->head;

    for (int i = skiplist->max_levels - 1; i >= 0; i--) {
        while (current->levels[i].next != NULL &&
               key_compare(current->levels[i].next->key, current->levels[i].next->key_size,
                          key, key_size) <= 0) {
            current = current->levels[i].next;
        }
    }

    if (current != skiplist->head &&
        key_compare(current->key, current->key_size, key, key_size) == 0) {
        // Check TTL if it's set
        if (current->ttl > 0 && current->ttl < time(NULL)) {
            return -1; // Key has expired
        }

        *value = current->value;
        *value_size = &current->value_size;
        return 0;
    }

    return -1;
}

int skiplist_delete(skiplist_t** skiplist, const uint8_t* key, size_t key_size,
                   const uint8_t* value, size_t value_size) {
    if (*skiplist == NULL || key == NULL) {
        return -1;
    }

    skiplist_node_t* update[(*skiplist)->max_levels];
    skiplist_node_t* current = (*skiplist)->head;

    // Find the node to delete
    for (int i = (*skiplist)->max_levels - 1; i >= 0; i--) {
        while (current->levels[i].next != NULL &&
               key_compare(current->levels[i].next->key, current->levels[i].next->key_size,
                          key, key_size) < 0) {
            current = current->levels[i].next;
        }
        update[i] = current;
    }

    current = current->levels[0].next;
    if (current == NULL ||
        key_compare(current->key, current->key_size, key, key_size) != 0) {
        return -1; // Key not found
    }

    // Optional value check
    if (value != NULL &&
        (current->value_size != value_size ||
         memcmp(current->value, value, value_size) != 0)) {
        return -1;
    }

    // Update forward pointers
    for (size_t i = 0; i < (*skiplist)->max_levels; i++) {
        if (update[i]->levels[i].next == current) {
            update[i]->levels[i].next = current->levels[i].next;

            // Update backward pointers at level 0
            if (i == 0 && current->levels[i].next != NULL) {
                current->levels[i].next->levels[i].prev = update[i];
            }
        }
    }

    // Update tail if we're deleting the last node
    if ((*skiplist)->tail == current) {
        (*skiplist)->tail = (current->levels[0].prev == (*skiplist)->head) ?
                            NULL : current->levels[0].prev;
    }

    free_node(current);
    (*skiplist)->size--;

    return 0;
}
