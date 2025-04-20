#ifndef __KVARKDB_FNVHASH__
#define __KVARKDB_FNVHASH__

#include <stdio.h>
#include <stdint.h>

// let's use fnv1 hashing
uint32_t fnv1a_hash(const char *key, size_t length) {
    uint32_t hash = 2166136261U;  // FNV offset basis
    for (size_t i = 0; i < length; i++) {
        hash ^= (uint32_t)key[i];
        hash *= 16777619U;  // FNV prime
    }
    return hash;
}

#endif
