#ifndef __KVARKDB_BLOOM_FILTER_H__
#define __KVARKDB_BLOOM_FILTER_H__

#include <stdint.h>
#include <stdio.h>

typedef struct {
    uint8_t *bit_array;
    size_t size;
    int num_hashes;
} BloomFilter;

// Initialize a Bloom filter with a given size and number of hash functions
BloomFilter *bloom_create(size_t size, int num_hashes);

// Add an element to the Bloom filter
void bloom_add(BloomFilter *bf, const char *item);

// Check if an element might be in the Bloom filter
int bloom_check(BloomFilter *bf, const char *item);

// Free the Bloom filter's allocated memory
void bloom_destroy(BloomFilter *bf);

#endif // BLOOM_FILTER_H
