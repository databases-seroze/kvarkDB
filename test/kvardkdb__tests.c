#include "../src/kvarkdb.h"
#include "test_macros.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define TEST_DB_PATH "/tmp/kvarkdb_test_db"

static void cleanup(void) {
    system("rm -rf " TEST_DB_PATH);
}

static void make_db(kvarkdb_t* db) {
    db->config.memtable_max_size   = 1024 * 1024;  /* 1 MB */
    db->config.sstable_target_size = 2 * 1024 * 1024;
    db->config.db_path             = TEST_DB_PATH;
    assert(kvarkdb_open(db) == 0);
}

/* ------------------------------------------------------------------ */

void test_open_close(void) {
    printf("Testing kvarkdb_open and kvarkdb_close... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);
    kvarkdb_close(&db);
    assert(db.column_families     == NULL);
    assert(db.column_family_count == 0);

    printf(GREEN "PASSED\n" RESET);
}

void test_open_null(void) {
    printf("Testing kvarkdb_open with NULL... ");
    assert(kvarkdb_open(NULL) == -1);
    printf(GREEN "PASSED\n" RESET);
}

void test_create_column_family(void) {
    printf("Testing kvarddb_create_column_family... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);

    assert(kvarddb_create_column_family(&db, "users")    == 0);
    assert(kvarddb_create_column_family(&db, "sessions") == 0);
    assert(db.column_family_count == 2);

    /* duplicate name rejected */
    assert(kvarddb_create_column_family(&db, "users") == -1);

    /* NULL / empty name rejected */
    assert(kvarddb_create_column_family(&db, NULL) == -1);
    assert(kvarddb_create_column_family(&db, "")   == -1);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

void test_drop_column_family(void) {
    printf("Testing kvarddb_drop_column_family... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);

    assert(kvarddb_create_column_family(&db, "a") == 0);
    assert(kvarddb_create_column_family(&db, "b") == 0);
    assert(kvarddb_create_column_family(&db, "c") == 0);

    assert(kvarddb_drop_column_family(&db, "b") == 0);
    assert(db.column_family_count == 2);

    /* "b" is gone */
    assert(kvarddb_drop_column_family(&db, "b") == -1);

    /* remaining CFs are intact */
    assert(kvarddb_drop_column_family(&db, "a") == 0);
    assert(kvarddb_drop_column_family(&db, "c") == 0);
    assert(db.column_family_count == 0);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

void test_put_get(void) {
    printf("Testing kvarkdb_put and kvarkdb_get... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);
    assert(kvarddb_create_column_family(&db, "default") == 0);

    assert(kvarkdb_put(&db, "default", "name", "alice") == 0);

    char* val = kvarkdb_get(&db, "default", "name");
    assert(val != NULL);
    assert(strcmp(val, "alice") == 0);
    free(val);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

void test_get_not_found(void) {
    printf("Testing kvarkdb_get key not found... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);
    assert(kvarddb_create_column_family(&db, "default") == 0);

    assert(kvarkdb_get(&db, "default", "ghost") == NULL);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

void test_put_update(void) {
    printf("Testing kvarkdb_put overwrites existing key... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);
    assert(kvarddb_create_column_family(&db, "default") == 0);

    assert(kvarkdb_put(&db, "default", "key", "old") == 0);
    assert(kvarkdb_put(&db, "default", "key", "new") == 0);

    char* val = kvarkdb_get(&db, "default", "key");
    assert(val != NULL);
    assert(strcmp(val, "new") == 0);
    free(val);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

void test_delete(void) {
    printf("Testing kvarkdb_delete... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);
    assert(kvarddb_create_column_family(&db, "default") == 0);

    assert(kvarkdb_put(&db,    "default", "key", "val") == 0);
    assert(kvarkdb_delete(&db, "default", "key")        == 0);
    assert(kvarkdb_get(&db,    "default", "key")        == NULL);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

void test_column_family_isolation(void) {
    printf("Testing column family key isolation... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);
    assert(kvarddb_create_column_family(&db, "cf1") == 0);
    assert(kvarddb_create_column_family(&db, "cf2") == 0);

    assert(kvarkdb_put(&db, "cf1", "key", "from_cf1") == 0);
    assert(kvarkdb_put(&db, "cf2", "key", "from_cf2") == 0);

    char* v1 = kvarkdb_get(&db, "cf1", "key");
    char* v2 = kvarkdb_get(&db, "cf2", "key");
    assert(strcmp(v1, "from_cf1") == 0);
    assert(strcmp(v2, "from_cf2") == 0);
    free(v1); free(v2);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

void test_wrong_column_family(void) {
    printf("Testing operations on nonexistent column family... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);

    assert(kvarkdb_put(&db,    "nosuchcf", "k", "v") == -1);
    assert(kvarkdb_get(&db,    "nosuchcf", "k")      == NULL);
    assert(kvarkdb_delete(&db, "nosuchcf", "k")      == -1);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

void test_flush_and_read_from_sstable(void) {
    printf("Testing flush to SSTable and read back... ");
    cleanup();
    kvarkdb_t db = {0};
    db.config.memtable_max_size   = 16;  /* tiny threshold to force flush (~4 puts) */
    db.config.sstable_target_size = 128;
    db.config.db_path             = TEST_DB_PATH;
    assert(kvarkdb_open(&db) == 0);
    assert(kvarddb_create_column_family(&db, "default") == 0);

    /* each put is ~9 bytes; after ~7 puts the 64-byte threshold triggers a flush */
    for (int i = 0; i < 10; i++) {
        char key[8], val[8];
        snprintf(key, sizeof(key), "k%d", i);
        snprintf(val, sizeof(val), "v%d", i);
        assert(kvarkdb_put(&db, "default", key, val) == 0);
    }

    /* at least one SSTable should have been flushed */
    assert(db.column_families[0].sst_count > 0);

    /* all keys must still be readable */
    for (int i = 0; i < 10; i++) {
        char key[8], expected[8];
        snprintf(key,      sizeof(key),      "k%d", i);
        snprintf(expected, sizeof(expected), "v%d", i);
        char* val = kvarkdb_get(&db, "default", key);
        assert(val != NULL);
        assert(strcmp(val, expected) == 0);
        free(val);
    }

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

void test_persist_and_reopen(void) {
    printf("Testing column families persist across reopen... ");
    cleanup();

    /* first open: create CFs and write data */
    {
        kvarkdb_t db = {0};
        make_db(&db);
        assert(kvarddb_create_column_family(&db, "persistent") == 0);
        assert(kvarkdb_put(&db, "persistent", "hello", "world") == 0);
        kvarkdb_close(&db);
    }

    /* second open: CFs and data should still be there */
    {
        kvarkdb_t db = {0};
        make_db(&db);

        /* "persistent" CF was loaded from disk */
        assert(db.column_family_count >= 1);

        char* val = kvarkdb_get(&db, "persistent", "hello");
        assert(val != NULL);
        assert(strcmp(val, "world") == 0);
        free(val);

        kvarkdb_close(&db);
    }

    printf(GREEN "PASSED\n" RESET);
}

/* delete a key that was flushed to SSTable — tombstone must suppress it */
void test_delete_after_flush(void) {
    printf("Testing kvarkdb_delete suppresses key flushed to SSTable... ");
    cleanup();
    kvarkdb_t db = {0};
    db.config.memtable_max_size   = 16;  /* tiny to force flush */
    db.config.sstable_target_size = 128;
    db.config.db_path             = TEST_DB_PATH;
    assert(kvarkdb_open(&db) == 0);
    assert(kvarddb_create_column_family(&db, "default") == 0);

    /* fill until at least one SSTable is flushed */
    for (int i = 0; i < 10; i++) {
        char key[8], val[8];
        snprintf(key, sizeof(key), "k%d", i);
        snprintf(val, sizeof(val), "v%d", i);
        assert(kvarkdb_put(&db, "default", key, val) == 0);
    }
    assert(db.column_families[0].sst_count > 0);

    /* delete a key that is definitely in an SSTable */
    assert(kvarkdb_delete(&db, "default", "k0") == 0);

    /* must not be visible anymore */
    assert(kvarkdb_get(&db, "default", "k0") == NULL);

    /* other keys unaffected */
    char* val = kvarkdb_get(&db, "default", "k1");
    assert(val != NULL);
    free(val);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

/* put → delete → put must restore the key */
void test_put_after_delete(void) {
    printf("Testing kvarkdb_put after delete restores key... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db);
    assert(kvarddb_create_column_family(&db, "default") == 0);

    assert(kvarkdb_put(&db,    "default", "key", "first") == 0);
    assert(kvarkdb_delete(&db, "default", "key")          == 0);
    assert(kvarkdb_get(&db,    "default", "key")          == NULL);

    assert(kvarkdb_put(&db, "default", "key", "second") == 0);

    char* val = kvarkdb_get(&db, "default", "key");
    assert(val != NULL);
    assert(strcmp(val, "second") == 0);
    free(val);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

int main(void) {
    printf(CYAN "Running kvarkdb tests...\n" RESET);

    test_open_close();
    test_open_null();
    test_create_column_family();
    test_drop_column_family();
    test_put_get();
    test_get_not_found();
    test_put_update();
    test_delete();
    test_column_family_isolation();
    test_wrong_column_family();
    test_flush_and_read_from_sstable();
    test_persist_and_reopen();
    test_delete_after_flush();
    test_put_after_delete();

    printf(GREEN "All kvarkdb tests passed.\n" RESET);
    return 0;
}
