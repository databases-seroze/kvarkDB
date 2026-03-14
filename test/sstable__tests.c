#include "../src/sstable.h"
#include "../src/memtable.h"
#include "test_macros.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_SST_PATH "/tmp/kvarkdb_test.sst"

static memtable_t *make_memtable(void) {
    memtable_t *mt = NULL;
    assert(memtable_new(&mt, 4, 0.5f) == 0);
    return mt;
}

static void put(memtable_t *mt, const char *key, const char *val) {
    assert(memtable_put(mt,
                        (const uint8_t *)key, strlen(key),
                        (uint8_t *)val, strlen(val),
                        0) == 0);
}

/* ------------------------------------------------------------------ */

void test_sstable_write_read(void) {
    printf("Testing sstable_write and sstable_get... ");

    memtable_t *mt = make_memtable();
    put(mt, "apple",  "red");
    put(mt, "banana", "yellow");
    put(mt, "cherry", "dark red");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL;
    size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"banana", 6, &val, &val_size) == 0);
    assert(val_size == 6);
    assert(memcmp(val, "yellow", 6) == 0);
    free(val);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_get_first_key(void) {
    printf("Testing sstable_get first key... ");

    memtable_t *mt = make_memtable();
    put(mt, "alpha", "1");
    put(mt, "beta",  "2");
    put(mt, "gamma", "3");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL;
    size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"alpha", 5, &val, &val_size) == 0);
    assert(memcmp(val, "1", 1) == 0);
    free(val);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_get_last_key(void) {
    printf("Testing sstable_get last key... ");

    memtable_t *mt = make_memtable();
    put(mt, "alpha", "1");
    put(mt, "beta",  "2");
    put(mt, "gamma", "3");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL;
    size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"gamma", 5, &val, &val_size) == 0);
    assert(memcmp(val, "3", 1) == 0);
    free(val);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_get_not_found(void) {
    printf("Testing sstable_get not found... ");

    memtable_t *mt = make_memtable();
    put(mt, "apple",  "red");
    put(mt, "banana", "yellow");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL;
    size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"mango", 5, &val, &val_size) == -1);
    assert(val == NULL);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_write_empty_memtable(void) {
    printf("Testing sstable_write with empty memtable... ");

    memtable_t *mt = make_memtable();
    assert(sstable_write(mt, TEST_SST_PATH) == -1);

    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_all_keys(void) {
    printf("Testing sstable_get all keys round-trip... ");

    const char *keys[] = {"cat", "dog", "eel", "fox", "gnu"};
    const char *vals[] = {"meow", "woof", "...", "bark", "moo"};
    size_t n = sizeof(keys) / sizeof(keys[0]);

    memtable_t *mt = make_memtable();
    for (size_t i = 0; i < n; i++) put(mt, keys[i], vals[i]);

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    for (size_t i = 0; i < n; i++) {
        uint8_t *val = NULL;
        size_t val_size = 0;
        assert(sstable_get(TEST_SST_PATH,
                           (const uint8_t *)keys[i], strlen(keys[i]),
                           &val, &val_size) == 0);
        assert(val_size == strlen(vals[i]));
        assert(memcmp(val, vals[i], val_size) == 0);
        free(val);
    }

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

int main(void) {
    printf(CYAN "Running sstable tests...\n" RESET);

    test_sstable_write_read();
    test_sstable_get_first_key();
    test_sstable_get_last_key();
    test_sstable_get_not_found();
    test_sstable_write_empty_memtable();
    test_sstable_all_keys();

    printf(GREEN "All sstable tests passed.\n" RESET);
    return 0;
}
