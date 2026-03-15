#include "../src/memtable.h"
#include "test_macros.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define MAX_LEVELS  4
#define PROBABILITY 0.5f

static memtable_t *make_memtable(void) {
    memtable_t *mt = NULL;
    assert(memtable_new(&mt, MAX_LEVELS, PROBABILITY) == 0);
    return mt;
}

static void put(memtable_t *mt, const char *key, const char *val) {
    assert(memtable_put(mt,
                        (const uint8_t *)key, strlen(key),
                        (uint8_t *)val, strlen(val),
                        0) == 0);
}

/* ------------------------------------------------------------------ */

void test_memtable_new(void) {
    printf("Testing memtable_new... ");

    memtable_t *mt = NULL;
    assert(memtable_new(&mt, MAX_LEVELS, PROBABILITY) == 0);
    assert(mt != NULL);
    assert(mt->skiplist != NULL);
    assert(mt->size_bytes == 0);
    assert(mt->immutable == false);

    memtable_destroy(&mt);
    assert(mt == NULL);
    printf(GREEN "PASSED\n" RESET);
}

void test_memtable_put_get(void) {
    printf("Testing memtable_put and memtable_get... ");

    memtable_t *mt = make_memtable();
    put(mt, "key", "value");

    uint8_t *val = NULL;
    size_t *val_size = NULL;
    assert(memtable_get(mt, (const uint8_t *)"key", 3, &val, &val_size, NULL) == 0);
    assert(*val_size == 5);
    assert(memcmp(val, "value", 5) == 0);

    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

void test_memtable_size_bytes(void) {
    printf("Testing memtable size_bytes tracking... ");

    memtable_t *mt = make_memtable();

    put(mt, "key", "val");  /* 3 + 3 = 6 */
    assert(mt->size_bytes == 6);

    put(mt, "ab", "cdef");  /* 2 + 4 = 6, total = 12 */
    assert(mt->size_bytes == 12);

    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

void test_memtable_size_bytes_on_update(void) {
    printf("Testing memtable size_bytes on update... ");

    memtable_t *mt = make_memtable();

    put(mt, "key", "old");   /* 3 + 3 = 6 */
    assert(mt->size_bytes == 6);

    put(mt, "key", "newval"); /* key already exists: 6 - 3 + 6 = 9 */
    assert(mt->size_bytes == 9);

    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

void test_memtable_delete(void) {
    printf("Testing memtable_delete... ");

    memtable_t *mt = make_memtable();
    put(mt, "key", "val");   /* key=3 + val=3 = 6 */
    assert(mt->size_bytes == 6);

    assert(memtable_delete(mt, (const uint8_t *)"key", 3) == 0);
    /* tombstone stays in skiplist: value gone (−3), key remains → size_bytes = 3 */
    assert(mt->size_bytes == 3);

    /* get returns the tombstone node — caller must check flags */
    uint8_t *val = NULL;
    size_t *val_size = NULL;
    uint8_t flags = 0;
    assert(memtable_get(mt, (const uint8_t *)"key", 3, &val, &val_size, &flags) == 0);
    assert(flags & SKIPLIST_FLAG_DELETED);

    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

void test_memtable_delete_nonexistent(void) {
    printf("Testing memtable_delete nonexistent key inserts tombstone... ");

    memtable_t *mt = make_memtable();
    /* key not in memtable — succeeds anyway (tombstone for SSTable key) */
    assert(memtable_delete(mt, (const uint8_t *)"ghost", 5) == 0);
    assert(mt->size_bytes == 5);  /* key_size only */

    uint8_t *val = NULL; size_t *val_size = NULL; uint8_t flags = 0;
    assert(memtable_get(mt, (const uint8_t *)"ghost", 5, &val, &val_size, &flags) == 0);
    assert(flags & SKIPLIST_FLAG_DELETED);

    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

void test_memtable_immutable(void) {
    printf("Testing memtable immutable blocks writes... ");

    memtable_t *mt = make_memtable();
    mt->immutable = true;

    assert(memtable_put(mt, (const uint8_t *)"k", 1,
                        (uint8_t *)"v", 1, 0) == -1);
    assert(memtable_delete(mt, (const uint8_t *)"k", 1) == -1);

    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

void test_memtable_needs_flush(void) {
    printf("Testing memtable_needs_flush... ");

    memtable_t *mt = make_memtable();
    put(mt, "key", "val");  /* size_bytes = 6 */

    assert(memtable_needs_flush(mt, 10) == false);
    assert(memtable_needs_flush(mt, 6)  == true);
    assert(memtable_needs_flush(mt, 5)  == true);

    memtable_destroy(&mt);
    printf(GREEN "PASSED\n" RESET);
}

int main(void) {
    printf(CYAN "Running memtable tests...\n" RESET);

    test_memtable_new();
    test_memtable_put_get();
    test_memtable_size_bytes();
    test_memtable_size_bytes_on_update();
    test_memtable_delete();
    test_memtable_delete_nonexistent();
    test_memtable_immutable();
    test_memtable_needs_flush();

    printf(GREEN "All memtable tests passed.\n" RESET);
    return 0;
}
