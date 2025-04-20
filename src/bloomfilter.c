
#include "bloomfilter.h"

#include <stdlib.h>
#include <string.h>
#include "fnv_hash.h"  // Assuming you're using FNV hash

// Create a new Bloom filter
BloomFilter *bloom_create(size_t size, int num_hashes) {
    BloomFilter *bf = (BloomFilter *)malloc(sizeof(BloomFilter));
    if (!bf) return NULL;

    bf->bit_array = (uint8_t *)calloc(size, sizeof(uint8_t));
    if (!bf->bit_array) {
        free(bf);
        return NULL;
    }

    bf->size = size;
    bf->num_hashes = num_hashes;
    return bf;
}

// Add an element to the Bloom filter
void bloom_add(BloomFilter *bf, const char *item) {
    for (int i = 0; i < bf->num_hashes; i++) {
        uint32_t hash = fnv1a_hash(item, strlen(item) + i); // Use FNV hash with varying seed
        size_t index = hash % bf->size;
        bf->bit_array[index] = 1;
    }
}

// Check if an element might be in the Bloom filter
int bloom_check(BloomFilter *bf, const char *item) {
    for (int i = 0; i < bf->num_hashes; i++) {
        uint32_t hash = fnv1a_hash(item, strlen(item) + i);
        size_t index = hash % bf->size;
        if (bf->bit_array[index] == 0) return 0; // Definitely not in the set
    }
    return 1; // Probably in the set
}

// Free the Bloom filter's allocated memory
void bloom_destroy(BloomFilter *bf) {
    free(bf->bit_array);
    free(bf);
}
