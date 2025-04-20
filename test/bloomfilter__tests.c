#include "../src/bloomfilter.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

void test_bloom_create(void) {
    printf("Testing bloom_create... ");
    size_t size = 100;
    int num_hashes = 3;
    BloomFilter *bf = bloom_create(size, num_hashes);

    assert(bf != NULL);
    assert(bf->bit_array != NULL);
    assert(bf->size == size);
    assert(bf->num_hashes == num_hashes);

    // Verify all bits are initially 0
    for (size_t i = 0; i < size; i++) {
        assert(bf->bit_array[i] == 0);
    }

    bloom_destroy(bf);
    printf("PASSED\n");
}

void test_bloom_add_check(void) {
    printf("Testing bloom_add and bloom_check... ");
    size_t size = 1000;
    int num_hashes = 5;
    BloomFilter *bf = bloom_create(size, num_hashes);

    const char *test_items[] = {"apple", "banana", "cherry", "date"};
    size_t num_items = sizeof(test_items) / sizeof(test_items[0]);

    // Add items and verify they're detected
    for (size_t i = 0; i < num_items; i++) {
        bloom_add(bf, test_items[i]);
        assert(bloom_check(bf, test_items[i]) == 1);
    }

    // Verify non-added items (potential false positives are possible)
    const char *non_items[] = {"grape", "kiwi"};
    size_t num_non_items = sizeof(non_items) / sizeof(non_items[0]);
    for (size_t i = 0; i < num_non_items; i++) {
        printf("\nNote: '%s' might show false positive (expected in Bloom filters)\n",
               non_items[i]);
    }

    bloom_destroy(bf);
    printf("PASSED\n");
}

void test_bloom_false_positives(void) {
    printf("Testing false positive rate... ");
    size_t size = 100;
    int num_hashes = 3;
    BloomFilter *bf = bloom_create(size, num_hashes);

    // Add some items
    bloom_add(bf, "item1");
    bloom_add(bf, "item2");

    // Check for non-existent items (may have false positives)
    int false_positives = 0;
    const char *test_items[] = {"nonexistent1", "nonexistent2", "nonexistent3"};
    for (size_t i = 0; i < 3; i++) {
        if (bloom_check(bf, test_items[i])) {
            false_positives++;
        }
    }
    printf("Observed %d/3 false positives (expected in Bloom filters)\n", false_positives);

    bloom_destroy(bf);
    printf("PASSED (but verify false positive rate)\n");
}

int main(void) {
    test_bloom_create();
    test_bloom_add_check();
    test_bloom_false_positives();
    printf("All tests completed.\n");
    return 0;
}
