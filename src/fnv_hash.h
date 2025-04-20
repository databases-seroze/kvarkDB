#ifndef __KVARKDB_FNVHASH__
#define __KVARKDB_FNVHASH__

#include <stdio.h>
#include <stdint.h>

// let's use fnv1 hashing
uint32_t fnv1a_hash(const char *key, size_t length);

#endif
