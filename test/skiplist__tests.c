#include "../src/skiplist.h"
#include "test_macros.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#define SL_MAX_LEVELS 4
#define SL_PROBABILITY 0.5f

/* helpers */
static skiplist_t *make_skiplist(void) {
    skiplist_t *sl = NULL;
    assert(skiplist_new(&sl, SL_MAX_LEVELS, SL_PROBABILITY) == 0);
    return sl;
}

static void put(skiplist_t **sl, const char *key, const char *val) {
    assert(skiplist_put(sl,
                        (const uint8_t *)key, strlen(key),
                        (uint8_t *)val, strlen(val),
                        0) == 0);
}

/* ------------------------------------------------------------------ */

void test_skiplist_new(void) {
    printf("Testing skiplist_new... ");

    skiplist_t *sl = NULL;
    assert(skiplist_new(&sl, SL_MAX_LEVELS, SL_PROBABILITY) == 0);
    assert(sl != NULL);
    assert(sl->size == 0);
    assert(sl->max_levels == SL_MAX_LEVELS);
    assert(sl->probability == SL_PROBABILITY);

    skiplist_clear(&sl);
    assert(sl == NULL);
    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_new_invalid_params(void) {
    printf("Testing skiplist_new invalid params... ");

    skiplist_t *sl = NULL;
    assert(skiplist_new(&sl, 0, 0.5f) == -1);   /* max_levels = 0   */
    assert(skiplist_new(&sl, 4, 0.0f) == -1);   /* probability = 0  */
    assert(skiplist_new(&sl, 4, 1.0f) == -1);   /* probability = 1  */
    assert(skiplist_new(&sl, 4, 1.5f) == -1);   /* probability > 1  */

    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_put_get(void) {
    printf("Testing skiplist_put and skiplist_get... ");

    skiplist_t *sl = make_skiplist();

    put(&sl, "hello", "world");

    uint8_t *val = NULL;
    size_t *val_size = NULL;
    assert(skiplist_get(sl, (const uint8_t *)"hello", 5, &val, &val_size) == 0);
    assert(val != NULL);
    assert(*val_size == 5);
    assert(memcmp(val, "world", 5) == 0);

    skiplist_clear(&sl);
    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_get_not_found(void) {
    printf("Testing skiplist_get not found... ");

    skiplist_t *sl = make_skiplist();
    put(&sl, "exists", "yes");

    uint8_t *val = NULL;
    size_t *val_size = NULL;
    assert(skiplist_get(sl, (const uint8_t *)"missing", 7, &val, &val_size) == -1);

    skiplist_clear(&sl);
    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_update_existing_key(void) {
    printf("Testing skiplist_put updates existing key... ");

    skiplist_t *sl = make_skiplist();
    put(&sl, "key", "old");
    put(&sl, "key", "new");

    assert(sl->size == 1);

    uint8_t *val = NULL;
    size_t *val_size = NULL;
    assert(skiplist_get(sl, (const uint8_t *)"key", 3, &val, &val_size) == 0);
    assert(*val_size == 3);
    assert(memcmp(val, "new", 3) == 0);

    skiplist_clear(&sl);
    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_delete(void) {
    printf("Testing skiplist_delete... ");

    skiplist_t *sl = make_skiplist();
    put(&sl, "alpha", "1");
    put(&sl, "beta",  "2");
    put(&sl, "gamma", "3");

    assert(sl->size == 3);
    assert(skiplist_delete(&sl, (const uint8_t *)"beta", 4, NULL, 0) == 0);
    assert(sl->size == 2);

    uint8_t *val = NULL;
    size_t *val_size = NULL;
    assert(skiplist_get(sl, (const uint8_t *)"beta", 4, &val, &val_size) == -1);

    /* remaining keys still present */
    assert(skiplist_get(sl, (const uint8_t *)"alpha", 5, &val, &val_size) == 0);
    assert(skiplist_get(sl, (const uint8_t *)"gamma", 5, &val, &val_size) == 0);

    skiplist_clear(&sl);
    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_delete_nonexistent(void) {
    printf("Testing skiplist_delete nonexistent key... ");

    skiplist_t *sl = make_skiplist();
    put(&sl, "real", "value");

    assert(skiplist_delete(&sl, (const uint8_t *)"ghost", 5, NULL, 0) == -1);
    assert(sl->size == 1);

    skiplist_clear(&sl);
    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_ttl_expired(void) {
    printf("Testing skiplist_get expired TTL... ");

    skiplist_t *sl = make_skiplist();

    /* TTL set to 1 second in the past */
    time_t expired = time(NULL) - 1;
    assert(skiplist_put(&sl,
                        (const uint8_t *)"key", 3,
                        (uint8_t *)"val", 3,
                        expired) == 0);

    uint8_t *val = NULL;
    size_t *val_size = NULL;
    assert(skiplist_get(sl, (const uint8_t *)"key", 3, &val, &val_size) == -1);

    skiplist_clear(&sl);
    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_ttl_valid(void) {
    printf("Testing skiplist_get valid TTL... ");

    skiplist_t *sl = make_skiplist();

    time_t future = time(NULL) + 9999;
    assert(skiplist_put(&sl,
                        (const uint8_t *)"key", 3,
                        (uint8_t *)"val", 3,
                        future) == 0);

    uint8_t *val = NULL;
    size_t *val_size = NULL;
    assert(skiplist_get(sl, (const uint8_t *)"key", 3, &val, &val_size) == 0);
    assert(memcmp(val, "val", 3) == 0);

    skiplist_clear(&sl);
    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_multiple_keys(void) {
    printf("Testing skiplist with multiple keys... ");

    skiplist_t *sl = make_skiplist();

    const char *keys[] = {"dog", "cat", "bird", "fish", "ant"};
    const char *vals[] = {"woof", "meow", "tweet", "blub", "..."};
    size_t n = sizeof(keys) / sizeof(keys[0]);

    for (size_t i = 0; i < n; i++) {
        put(&sl, keys[i], vals[i]);
    }
    assert(sl->size == n);

    for (size_t i = 0; i < n; i++) {
        uint8_t *val = NULL;
        size_t *val_size = NULL;
        assert(skiplist_get(sl,
                            (const uint8_t *)keys[i], strlen(keys[i]),
                            &val, &val_size) == 0);
        assert(*val_size == strlen(vals[i]));
        assert(memcmp(val, vals[i], *val_size) == 0);
    }

    skiplist_clear(&sl);
    printf(GREEN "PASSED\n" RESET);
}

void test_skiplist_clear(void) {
    printf("Testing skiplist_clear... ");

    skiplist_t *sl = make_skiplist();
    put(&sl, "a", "1");
    put(&sl, "b", "2");

    assert(skiplist_clear(&sl) == 0);
    assert(sl == NULL);

    printf(GREEN "PASSED\n" RESET);
}

int main(void) {
    printf(CYAN "Running skiplist tests...\n" RESET);

    test_skiplist_new();
    test_skiplist_new_invalid_params();
    test_skiplist_put_get();
    test_skiplist_get_not_found();
    test_skiplist_update_existing_key();
    test_skiplist_delete();
    test_skiplist_delete_nonexistent();
    test_skiplist_ttl_expired();
    test_skiplist_ttl_valid();
    test_skiplist_multiple_keys();
    test_skiplist_clear();

    printf(GREEN "All skiplist tests passed.\n" RESET);
    return 0;
}
