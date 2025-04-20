#ifndef __KVARKDB_FNVHASH_H__
#define __KVARKDB_FNVHASH_H__

#include <stdio.h>
#include <stdint.h>

// let's use fnv1 hashing
uint32_t fnv1a_hash(const char *key, size_t length);

#endif
