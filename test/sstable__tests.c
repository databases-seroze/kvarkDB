#include "../src/sstable.h"
#include "../src/memtable.h"
#include "test_macros.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_SST_PATH  "/tmp/kvarkdb_test.sst"
#define TEST_SST_PATH2 "/tmp/kvarkdb_test2.sst"

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

    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"banana", 6, &val, &val_size, NULL) == 0);
    assert(val_size == 6);
    assert(memcmp(val, "yellow", 6) == 0);
    free(val);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_get_first_key(void) {
    printf("Testing sstable_get first key (binary search edge)... ");

    memtable_t *mt = make_memtable();
    put(mt, "alpha", "1");
    put(mt, "beta",  "2");
    put(mt, "gamma", "3");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"alpha", 5, &val, &val_size, NULL) == 0);
    assert(val_size == 1);
    assert(memcmp(val, "1", 1) == 0);
    free(val);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_get_last_key(void) {
    printf("Testing sstable_get last key (binary search edge)... ");

    memtable_t *mt = make_memtable();
    put(mt, "alpha", "1");
    put(mt, "beta",  "2");
    put(mt, "gamma", "3");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"gamma", 5, &val, &val_size, NULL) == 0);
    assert(val_size == 1);
    assert(memcmp(val, "3", 1) == 0);
    free(val);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_get_not_found(void) {
    printf("Testing sstable_get key not found... ");

    memtable_t *mt = make_memtable();
    put(mt, "apple",  "red");
    put(mt, "banana", "yellow");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"mango", 5, &val, &val_size, NULL) == -1);
    assert(val == NULL);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_write_null_memtable(void) {
    printf("Testing sstable_write with NULL memtable... ");
    assert(sstable_write(NULL, TEST_SST_PATH) == -1);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_write_null_path(void) {
    printf("Testing sstable_write with NULL path... ");
    memtable_t *mt = make_memtable();
    put(mt, "key", "val");
    assert(sstable_write(mt, NULL) == -1);
    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_write_empty_memtable(void) {
    printf("Testing sstable_write with empty memtable... ");
    memtable_t *mt = make_memtable();
    assert(sstable_write(mt, TEST_SST_PATH) == -1);
    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_get_null_path(void) {
    printf("Testing sstable_get with NULL path... ");
    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get(NULL, (const uint8_t *)"key", 3, &val, &val_size, NULL) == -1);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_get_null_key(void) {
    printf("Testing sstable_get with NULL key... ");
    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, NULL, 3, &val, &val_size, NULL) == -1);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_get_nonexistent_file(void) {
    printf("Testing sstable_get on nonexistent file... ");
    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get("/tmp/does_not_exist_kvark.sst",
                       (const uint8_t *)"key", 3, &val, &val_size, NULL) == -1);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_single_entry(void) {
    printf("Testing sstable with single entry... ");

    memtable_t *mt = make_memtable();
    put(mt, "onlyone", "value");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"onlyone", 7, &val, &val_size, NULL) == 0);
    assert(val_size == 5);
    assert(memcmp(val, "value", 5) == 0);
    free(val);

    /* non-existent key in single-entry table */
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"other", 5, &val, &val_size, NULL) == -1);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_large_value(void) {
    printf("Testing sstable with large value (10KB)... ");

    size_t large_size = 10 * 1024;
    uint8_t *large_val = malloc(large_size);
    assert(large_val != NULL);
    memset(large_val, 0xAB, large_size);

    memtable_t *mt = make_memtable();
    assert(memtable_put(mt, (const uint8_t *)"bigkey", 6, large_val, large_size, 0) == 0);
    free(large_val);

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"bigkey", 6, &val, &val_size, NULL) == 0);
    assert(val_size == large_size);
    for (size_t i = 0; i < large_size; i++) assert(val[i] == 0xAB);
    free(val);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_key_prefix(void) {
    printf("Testing sstable key prefix not confused with full key... ");

    memtable_t *mt = make_memtable();
    put(mt, "app",    "short");
    put(mt, "apple",  "full");
    put(mt, "applet", "longer");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL; size_t val_size = 0;

    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"app",    3, &val, &val_size, NULL) == 0);
    assert(memcmp(val, "short", 5) == 0); free(val); val = NULL;

    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"apple",  5, &val, &val_size, NULL) == 0);
    assert(memcmp(val, "full", 4) == 0); free(val); val = NULL;

    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"applet", 6, &val, &val_size, NULL) == 0);
    assert(memcmp(val, "longer", 6) == 0); free(val); val = NULL;

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_value_is_heap_allocated(void) {
    printf("Testing sstable_get returns heap-allocated value... ");

    memtable_t *mt = make_memtable();
    put(mt, "key", "val");

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    uint8_t *val = NULL; size_t val_size = 0;
    assert(sstable_get(TEST_SST_PATH, (const uint8_t *)"key", 3, &val, &val_size, NULL) == 0);

    /* write to the returned buffer — would segfault if it weren't heap-allocated */
    val[0] = 'X';
    free(val);

    unlink(TEST_SST_PATH);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_independent_files(void) {
    printf("Testing two independent sstable files... ");

    memtable_t *mt1 = make_memtable();
    put(mt1, "key", "from_table_1");
    assert(sstable_write(mt1, TEST_SST_PATH) == 0);
    memtable_destroy(&mt1);

    memtable_t *mt2 = make_memtable();
    put(mt2, "key", "from_table_2");
    assert(sstable_write(mt2, TEST_SST_PATH2) == 0);
    memtable_destroy(&mt2);

    uint8_t *val = NULL; size_t val_size = 0;

    assert(sstable_get(TEST_SST_PATH,  (const uint8_t *)"key", 3, &val, &val_size, NULL) == 0);
    assert(memcmp(val, "from_table_1", 12) == 0); free(val); val = NULL;

    assert(sstable_get(TEST_SST_PATH2, (const uint8_t *)"key", 3, &val, &val_size, NULL) == 0);
    assert(memcmp(val, "from_table_2", 12) == 0); free(val);

    unlink(TEST_SST_PATH);
    unlink(TEST_SST_PATH2);
    printf(GREEN "PASSED\n" RESET);
}

void test_sstable_stress(void) {
    printf("Testing sstable with 50 entries... ");

    memtable_t *mt = make_memtable();
    char key[32], val[32];
    for (int i = 0; i < 50; i++) {
        snprintf(key, sizeof(key), "key_%03d", i);
        snprintf(val, sizeof(val), "value_%03d", i);
        put(mt, key, val);
    }

    assert(sstable_write(mt, TEST_SST_PATH) == 0);
    memtable_destroy(&mt);

    for (int i = 0; i < 50; i++) {
        snprintf(key, sizeof(key), "key_%03d", i);
        snprintf(val, sizeof(val), "value_%03d", i);

        uint8_t *got = NULL; size_t got_size = 0;
        assert(sstable_get(TEST_SST_PATH,
                           (const uint8_t *)key, strlen(key),
                           &got, &got_size, NULL) == 0);
        assert(got_size == strlen(val));
        assert(memcmp(got, val, got_size) == 0);
        free(got);
    }

    unlink(TEST_SST_PATH);
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
        uint8_t *val = NULL; size_t val_size = 0;
        assert(sstable_get(TEST_SST_PATH,
                           (const uint8_t *)keys[i], strlen(keys[i]),
                           &val, &val_size, NULL) == 0);
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
    test_sstable_write_null_memtable();
    test_sstable_write_null_path();
    test_sstable_write_empty_memtable();
    test_sstable_get_null_path();
    test_sstable_get_null_key();
    test_sstable_get_nonexistent_file();
    test_sstable_single_entry();
    test_sstable_large_value();
    test_sstable_key_prefix();
    test_sstable_value_is_heap_allocated();
    test_sstable_independent_files();
    test_sstable_stress();
    test_sstable_all_keys();

    printf(GREEN "All sstable tests passed.\n" RESET);
    return 0;
}
