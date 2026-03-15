#include "../src/kvarkdb.h"
#include "../src/sstable.h"
#include "../src/memtable.h"
#include "test_macros.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define TEST_DB_PATH "/tmp/kvarkdb_compaction_test"

static void cleanup(void) {
    system("rm -rf " TEST_DB_PATH);
}

/* open a DB with a tiny memtable and a low compaction threshold */
static void make_db(kvarkdb_t* db, size_t memtable_max, size_t compaction_threshold) {
    db->config.memtable_max_size   = memtable_max;
    db->config.sstable_target_size = memtable_max * 2;
    db->config.compaction_threshold = compaction_threshold;
    db->config.db_path             = TEST_DB_PATH;
    assert(kvarkdb_open(db) == 0);
}

/* ------------------------------------------------------------------ */

/*
 * Force enough flushes to hit the compaction threshold, then verify that the
 * SSTable count drops back to 1 and all live keys are still readable.
 */
void test_auto_compaction_triggered(void) {
    printf("Testing auto-compaction triggers and reduces SSTable count... ");
    cleanup();
    kvarkdb_t db = {0};
    /* threshold=2: compact after the 2nd SSTable is flushed */
    make_db(&db, 16, 2);
    assert(kvarddb_create_column_family(&db, "cf") == 0);

    /* write enough keys to force multiple flushes */
    for (int i = 0; i < 20; i++) {
        char key[16], val[16];
        snprintf(key, sizeof(key), "key%02d", i);
        snprintf(val, sizeof(val), "val%02d", i);
        assert(kvarkdb_put(&db, "cf", key, val) == 0);
    }

    /* compaction must have fired — at most 1 SSTable should remain */
    assert(db.column_families[0].sst_count <= 1);

    /* every key still readable */
    for (int i = 0; i < 20; i++) {
        char key[16], expected[16];
        snprintf(key,      sizeof(key),      "key%02d", i);
        snprintf(expected, sizeof(expected), "val%02d", i);
        char* v = kvarkdb_get(&db, "cf", key);
        assert(v != NULL);
        assert(strcmp(v, expected) == 0);
        free(v);
    }

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

/*
 * Write a key, flush it to an SSTable, then overwrite it and compact.
 * The compacted SSTable should contain only the newest value.
 */
void test_compaction_deduplicates_keys(void) {
    printf("Testing compaction keeps only newest value for duplicate keys... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db, 16, 4);   /* high threshold so we control compaction manually */
    assert(kvarddb_create_column_family(&db, "cf") == 0);

    /* flush "key" → "old" into SST 1 */
    assert(kvarkdb_put(&db, "cf", "aaa", "old") == 0);
    for (int i = 0; i < 5; i++) {
        char k[8]; snprintf(k, sizeof(k), "p%d", i);
        kvarkdb_put(&db, "cf", k, "pad");   /* pad to trigger flush */
    }
    assert(db.column_families[0].sst_count >= 1);
    size_t sst_before = db.column_families[0].sst_count;

    /* overwrite "key" with "new" — this goes into a newer SSTable */
    assert(kvarkdb_put(&db, "cf", "aaa", "new") == 0);
    for (int i = 0; i < 5; i++) {
        char k[8]; snprintf(k, sizeof(k), "q%d", i);
        kvarkdb_put(&db, "cf", k, "pad");
    }
    /* make sure we got at least one more SSTable */
    assert(db.column_families[0].sst_count > sst_before);

    /* manually compact by lowering threshold */
    db.config.compaction_threshold = 1;
    /* force one more flush to trigger compaction */
    for (int i = 0; i < 5; i++) {
        char k[8]; snprintf(k, sizeof(k), "r%d", i);
        kvarkdb_put(&db, "cf", k, "pad");
    }

    /* only the newest value should survive */
    char* v = kvarkdb_get(&db, "cf", "aaa");
    assert(v != NULL);
    assert(strcmp(v, "new") == 0);
    free(v);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

/*
 * Delete a key, let it flush to an SSTable, compact, then verify the key
 * is gone (tombstone was dropped and the key is no longer visible).
 */
void test_compaction_removes_tombstones(void) {
    printf("Testing compaction drops tombstones so deleted keys stay gone... ");
    cleanup();
    kvarkdb_t db = {0};
    make_db(&db, 16, 2);
    assert(kvarddb_create_column_family(&db, "cf") == 0);

    /* write and flush key → first SSTable */
    assert(kvarkdb_put(&db, "cf", "gone", "here") == 0);
    for (int i = 0; i < 5; i++) {
        char k[8]; snprintf(k, sizeof(k), "x%d", i);
        kvarkdb_put(&db, "cf", k, "val");
    }

    /* delete key → will be in the next SSTable as a tombstone */
    assert(kvarkdb_delete(&db, "cf", "gone") == 0);
    for (int i = 0; i < 5; i++) {
        char k[8]; snprintf(k, sizeof(k), "y%d", i);
        kvarkdb_put(&db, "cf", k, "val");   /* triggers flush + compaction */
    }

    /* after compaction the key must be gone */
    assert(kvarkdb_get(&db, "cf", "gone") == NULL);

    kvarkdb_close(&db);
    printf(GREEN "PASSED\n" RESET);
}

/*
 * Verify that after compaction and a DB reopen, the manifest is correct and
 * all live keys are still readable.
 */
void test_compaction_persists_across_reopen(void) {
    printf("Testing data survives compaction + close + reopen... ");
    cleanup();

    {
        kvarkdb_t db = {0};
        make_db(&db, 16, 2);
        assert(kvarddb_create_column_family(&db, "cf") == 0);

        for (int i = 0; i < 20; i++) {
            char k[16], v[16];
            snprintf(k, sizeof(k), "k%02d", i);
            snprintf(v, sizeof(v), "v%02d", i);
            assert(kvarkdb_put(&db, "cf", k, v) == 0);
        }
        kvarkdb_close(&db);
    }

    /* reopen and verify */
    {
        kvarkdb_t db = {0};
        make_db(&db, 16, 2);
        assert(db.column_family_count >= 1);

        for (int i = 0; i < 20; i++) {
            char k[16], expected[16];
            snprintf(k,        sizeof(k),        "k%02d", i);
            snprintf(expected, sizeof(expected), "v%02d", i);
            char* v = kvarkdb_get(&db, "cf", k);
            assert(v != NULL);
            assert(strcmp(v, expected) == 0);
            free(v);
        }
        kvarkdb_close(&db);
    }

    printf(GREEN "PASSED\n" RESET);
}

static void mt_put(memtable_t* mt, const char* key, const char* val) {
    assert(memtable_put(mt,
                        (const uint8_t*)key, strlen(key),
                        (uint8_t*)val,       strlen(val), 0) == 0);
}

static void mt_delete(memtable_t* mt, const char* key) {
    assert(memtable_delete(mt, (const uint8_t*)key, strlen(key)) == 0);
}

static memtable_t* new_mt(void) {
    memtable_t* mt = NULL;
    assert(memtable_new(&mt, 4, 0.5f) == 0);
    return mt;
}

/*
 * Direct unit test for sstable_merge: build two SSTables from memtables,
 * merge them, and verify the output.
 */
void test_sstable_merge_direct(void) {
    printf("Testing sstable_merge directly (two files → one)... ");
    cleanup();
    system("mkdir -p " TEST_DB_PATH);

    const char* sst1 = TEST_DB_PATH "/1.sst";
    const char* sst2 = TEST_DB_PATH "/2.sst";
    const char* out  = TEST_DB_PATH "/merged.sst";

    /* SST 1 (older): a=old, b, c */
    memtable_t* mt1 = new_mt();
    mt_put(mt1, "a", "val_a_old");
    mt_put(mt1, "b", "val_b");
    mt_put(mt1, "c", "val_c");
    assert(sstable_write(mt1, sst1) == 0);
    memtable_destroy(&mt1);

    /* SST 2 (newer): a=new (update), d=new */
    memtable_t* mt2 = new_mt();
    mt_put(mt2, "a", "val_a_new");
    mt_put(mt2, "d", "val_d");
    assert(sstable_write(mt2, sst2) == 0);
    memtable_destroy(&mt2);

    /* merge: sst1 (older) then sst2 (newer) */
    const char* paths[2] = { sst1, sst2 };
    assert(sstable_merge(paths, 2, out) == 0);

    /* "a" should have the new value from SST2 */
    uint8_t* val = NULL; size_t val_sz = 0; uint8_t flags = 0;
    assert(sstable_get(out, (const uint8_t*)"a", 1, &val, &val_sz, &flags) == 0);
    assert(!(flags & 0x01));
    assert(val_sz == strlen("val_a_new"));
    assert(memcmp(val, "val_a_new", val_sz) == 0);
    free(val); val = NULL;

    /* "b" and "c" from SST1, "d" from SST2 — all present */
    assert(sstable_get(out, (const uint8_t*)"b", 1, &val, &val_sz, &flags) == 0); free(val); val = NULL;
    assert(sstable_get(out, (const uint8_t*)"c", 1, &val, &val_sz, &flags) == 0); free(val); val = NULL;
    assert(sstable_get(out, (const uint8_t*)"d", 1, &val, &val_sz, &flags) == 0); free(val);

    unlink(sst1); unlink(sst2); unlink(out);
    printf(GREEN "PASSED\n" RESET);
}

/*
 * Tombstone suppression test: SST1 has key "gone", SST2 has a tombstone for it.
 * After merge, "gone" must not appear.
 */
void test_sstable_merge_tombstone_suppression(void) {
    printf("Testing sstable_merge drops tombstones... ");
    cleanup();
    system("mkdir -p " TEST_DB_PATH);

    const char* sst1 = TEST_DB_PATH "/t1.sst";
    const char* sst2 = TEST_DB_PATH "/t2.sst";
    const char* out  = TEST_DB_PATH "/t_merged.sst";

    /* SST 1 (older): key "gone" has a value */
    memtable_t* mt1 = new_mt();
    mt_put(mt1, "keep", "alive");
    mt_put(mt1, "gone", "here");
    assert(sstable_write(mt1, sst1) == 0);
    memtable_destroy(&mt1);

    /* SST 2 (newer): "gone" is deleted (tombstone), "keep" updated */
    memtable_t* mt2 = new_mt();
    mt_put(mt2,    "keep", "updated");
    mt_delete(mt2, "gone");
    assert(sstable_write(mt2, sst2) == 0);
    memtable_destroy(&mt2);

    const char* paths[2] = { sst1, sst2 };
    assert(sstable_merge(paths, 2, out) == 0);

    uint8_t* val = NULL; size_t val_sz = 0; uint8_t flags = 0;

    /* "keep" must be present with the updated value */
    assert(sstable_get(out, (const uint8_t*)"keep", 4, &val, &val_sz, &flags) == 0);
    assert(memcmp(val, "updated", val_sz) == 0);
    free(val); val = NULL;

    /* "gone" must NOT be present */
    assert(sstable_get(out, (const uint8_t*)"gone", 4, &val, &val_sz, &flags) == -1);

    unlink(sst1); unlink(sst2); unlink(out);
    printf(GREEN "PASSED\n" RESET);
}

/*
 * All-tombstone merge: if every key in all SSTables is a tombstone (or their
 * only version is a tombstone), sstable_merge should return 1 (no output file).
 */
void test_sstable_merge_all_tombstones(void) {
    printf("Testing sstable_merge returns 1 when all entries are tombstones... ");
    cleanup();
    system("mkdir -p " TEST_DB_PATH);

    const char* sst1 = TEST_DB_PATH "/all1.sst";
    const char* sst2 = TEST_DB_PATH "/all2.sst";
    const char* out  = TEST_DB_PATH "/all_merged.sst";

    /* SST1: write "x" then delete it in SST2 */
    memtable_t* mt1 = new_mt();
    mt_put(mt1, "x", "val");
    assert(sstable_write(mt1, sst1) == 0);
    memtable_destroy(&mt1);

    memtable_t* mt2 = new_mt();
    mt_delete(mt2, "x");
    assert(sstable_write(mt2, sst2) == 0);
    memtable_destroy(&mt2);

    const char* paths[2] = { sst1, sst2 };
    int rc = sstable_merge(paths, 2, out);
    assert(rc == 1);                   /* 1 = success, nothing to write */
    assert(access(out, F_OK) == -1);   /* output file must NOT exist */

    unlink(sst1); unlink(sst2);
    printf(GREEN "PASSED\n" RESET);
}

/* ------------------------------------------------------------------ */

int main(void) {
    printf(CYAN "Running compaction tests...\n" RESET);

    test_auto_compaction_triggered();
    test_compaction_deduplicates_keys();
    test_compaction_removes_tombstones();
    test_compaction_persists_across_reopen();
    test_sstable_merge_direct();
    test_sstable_merge_tombstone_suppression();
    test_sstable_merge_all_tombstones();

    printf(GREEN "All compaction tests passed.\n" RESET);
    return 0;
}
