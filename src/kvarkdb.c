#include "kvarkdb.h"
#include "sstable.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

/* maximum supported path length */
#define KVARKDB_MAX_PATH 1024

/* compact a column family when its SSTable count reaches this threshold */
#define KVARKDB_DEFAULT_COMPACTION_THRESHOLD 4

/* WAL rotates to a new file once this size is exceeded */
#define KVARKDB_WAL_MAX_SIZE (64 * 1024 * 1024)  /* 64 MiB */


#define DATA_DIR             "data"
#define META_DIR             "meta"
#define SSTABLES_DIR         "sstables"
#define COLUMN_FAMILIES_FILE "meta/column_families"
#define MANIFEST_FILE        "meta/manifest"

/* ------------------------------------------------------------------ */
/* internal helpers                                                     */

static int make_dir(const char* path) {
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        perror(path);
        return -1;
    }
    errno = 0;
    return 0;
}

static int create_db_directories(const char* path) {
    char buf[KVARKDB_MAX_PATH];

    if (make_dir(path) != 0) return -1;

    if (snprintf(buf, sizeof(buf), "%s/%s", path, DATA_DIR) >= (int)sizeof(buf)) return -1;
    if (make_dir(buf) != 0) return -1;

    if (snprintf(buf, sizeof(buf), "%s/%s/%s", path, DATA_DIR, META_DIR) >= (int)sizeof(buf)) return -1;
    if (make_dir(buf) != 0) return -1;

    if (snprintf(buf, sizeof(buf), "%s/%s/%s", path, DATA_DIR, SSTABLES_DIR) >= (int)sizeof(buf)) return -1;
    if (make_dir(buf) != 0) return -1;

    return 0;
}

static kvarkdb_column_family_t* find_cf(kvarkdb_t* db, const char* name) {
    for (size_t i = 0; i < db->column_family_count; i++) {
        if (strcmp(db->column_families[i].name, name) == 0)
            return &db->column_families[i];
    }
    return NULL;
}

/* write all CF names to data/meta/column_families atomically */
static int persist_column_families(kvarkdb_t* db) {
    char path[KVARKDB_MAX_PATH], tmp[KVARKDB_MAX_PATH];

    if (snprintf(path, sizeof(path), "%s/%s/%s",
                 db->config.db_path, DATA_DIR, COLUMN_FAMILIES_FILE) >= (int)sizeof(path)) return -1;
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return -1;

    FILE* f = fopen(tmp, "w");
    if (!f) return -1;

    for (size_t i = 0; i < db->column_family_count; i++) {
        if (fprintf(f, "%s\n", db->column_families[i].name) < 0) {
            fclose(f); return -1;
        }
    }
    fclose(f);
    return rename(tmp, path);
}

/* rewrite data/meta/manifest from scratch reflecting current cf->sst_paths state */
static int rewrite_manifest(kvarkdb_t* db) {
    char path[KVARKDB_MAX_PATH], tmp[KVARKDB_MAX_PATH];
    if (snprintf(path, sizeof(path), "%s/%s/%s",
                 db->config.db_path, DATA_DIR, MANIFEST_FILE) >= (int)sizeof(path)) return -1;
    if (snprintf(tmp, sizeof(tmp), "%s.tmp", path) >= (int)sizeof(tmp)) return -1;

    FILE* f = fopen(tmp, "w");
    if (!f) return -1;

    for (size_t i = 0; i < db->column_family_count; i++) {
        kvarkdb_column_family_t* cf = &db->column_families[i];
        for (size_t j = 0; j < cf->sst_count; j++) {
            if (fprintf(f, "%s %s\n", cf->name, cf->sst_paths[j]) < 0) {
                fclose(f); return -1;
            }
        }
    }

    fclose(f);
    return rename(tmp, path);
}

/* append one SSTable entry to data/meta/manifest */
static int append_manifest(kvarkdb_t* db, const char* cf_name, const char* sst_path) {
    char path[KVARKDB_MAX_PATH];
    if (snprintf(path, sizeof(path), "%s/%s/%s",
                 db->config.db_path, DATA_DIR, MANIFEST_FILE) >= (int)sizeof(path)) return -1;

    FILE* f = fopen(path, "a");
    if (!f) return -1;
    int rc = fprintf(f, "%s %s\n", cf_name, sst_path);
    fclose(f);
    return rc < 0 ? -1 : 0;
}

/*
 * compact_column_family — merges all SSTables for a CF into one.
 * removes old SSTable files, updates cf->sst_paths, and rewrites the manifest.
 * called automatically from flush_memtable when the SSTable count hits the threshold.
 */
static int compact_column_family(kvarkdb_t* db, kvarkdb_column_family_t* cf) {
    if (cf->sst_count < 2) return 0;

    char out_path[KVARKDB_MAX_PATH];
    if (snprintf(out_path, sizeof(out_path), "%s/%s/%s/%s/%lu.sst",
                 db->config.db_path, DATA_DIR, SSTABLES_DIR, cf->name,
                 (unsigned long)cf->next_sst_seq) >= (int)sizeof(out_path)) return -1;

    int rc = sstable_merge((const char**)cf->sst_paths, cf->sst_count, out_path);
    if (rc == -1) return -1;

    /* remove old SSTable files from disk */
    for (size_t i = 0; i < cf->sst_count; i++) {
        remove(cf->sst_paths[i]);
        free(cf->sst_paths[i]);
    }
    free(cf->sst_paths);
    cf->sst_paths = NULL;
    cf->sst_count = 0;
    cf->next_sst_seq++;

    if (rc == 0) {
        /* output file was written — register it */
        cf->sst_paths = malloc(sizeof(char*));
        if (!cf->sst_paths) return -1;
        cf->sst_paths[0] = strdup(out_path);
        if (!cf->sst_paths[0]) { free(cf->sst_paths); cf->sst_paths = NULL; return -1; }
        cf->sst_count = 1;
    }
    /* rc == 1 means all entries were tombstones: sst_count stays 0, no file created */

    rewrite_manifest(db);
    return 0;
}

/*
 * flush_memtable — writes the CF's active memtable to a new SSTable,
 * appends the path to cf->sst_paths, updates the manifest, then
 * swaps in a fresh empty memtable.
 */
static int flush_memtable(kvarkdb_t* db, kvarkdb_column_family_t* cf) {
    if (cf->memtable->skiplist->size == 0) return 0;

    cf->memtable->immutable = true;

    char sst_path[KVARKDB_MAX_PATH];
    if (snprintf(sst_path, sizeof(sst_path), "%s/%s/%s/%s/%lu.sst",
                 db->config.db_path, DATA_DIR, SSTABLES_DIR, cf->name,
                 (unsigned long)cf->next_sst_seq) >= (int)sizeof(sst_path)) {
        cf->memtable->immutable = false;
        return -1;
    }

    if (sstable_write(cf->memtable, sst_path) != 0) {
        cf->memtable->immutable = false;
        return -1;
    }

    char** new_paths = realloc(cf->sst_paths, (cf->sst_count + 1) * sizeof(char*));
    if (!new_paths) { cf->memtable->immutable = false; return -1; }
    cf->sst_paths = new_paths;
    cf->sst_paths[cf->sst_count] = strdup(sst_path);
    if (!cf->sst_paths[cf->sst_count]) { cf->memtable->immutable = false; return -1; }
    cf->sst_count++;
    cf->next_sst_seq++;

    append_manifest(db, cf->name, sst_path);

    memtable_destroy(&cf->memtable);
    if (memtable_new(&cf->memtable, 12, 0.5f) != 0) return -1;

    /* trigger compaction when SSTable count reaches the configured threshold */
    size_t threshold = db->config.compaction_threshold
                       ? db->config.compaction_threshold
                       : KVARKDB_DEFAULT_COMPACTION_THRESHOLD;
    if (cf->sst_count >= threshold)
        return compact_column_family(db, cf);

    return 0;
}

static void free_cf_contents(kvarkdb_column_family_t* cf) {
    if (cf->memtable) memtable_destroy(&cf->memtable);
    for (size_t j = 0; j < cf->sst_count; j++) free(cf->sst_paths[j]);
    free(cf->sst_paths);
    free(cf->name);
}

/* ------------------------------------------------------------------ */
/* WAL replay callback                                                  */

static int wal_replay_cb(const wal_record_t* rec, void* ctx) {
    kvarkdb_t* db = (kvarkdb_t*)ctx;
    kvarkdb_column_family_t* cf = find_cf(db, rec->cf_name);
    if (!cf) return 0;  /* unknown CF — skip */

    if (rec->op_type == WAL_OP_PUT) {
        memtable_put(cf->memtable,
                     rec->key,               rec->key_size,
                     (uint8_t*)rec->value,   rec->value_size, 0);
    } else if (rec->op_type == WAL_OP_DELETE) {
        memtable_delete(cf->memtable, rec->key, rec->key_size);
    }
    return 0;
}

/* ------------------------------------------------------------------ */
/* public API                                                           */

int kvarkdb_open(kvarkdb_t* db) {
    if (!db || !db->config.db_path) return -1;

    db->column_families       = NULL;
    db->column_family_count   = 0;
    db->wal                   = NULL;

    if (create_db_directories(db->config.db_path) != 0) return -1;

    /* load column families */
    char cf_path[KVARKDB_MAX_PATH];
    if (snprintf(cf_path, sizeof(cf_path), "%s/%s/%s",
                 db->config.db_path, DATA_DIR, COLUMN_FAMILIES_FILE) >= (int)sizeof(cf_path)) return -1;

    FILE* cf_file = fopen(cf_path, "r");
    if (cf_file) {
        char line[256];
        while (fgets(line, sizeof(line), cf_file)) {
            line[strcspn(line, "\n")] = '\0';
            if (line[0] == '\0') continue;
            if (kvarddb_create_column_family(db, line) != 0) {
                fclose(cf_file);
                kvarkdb_close(db);
                return -1;
            }
        }
        fclose(cf_file);
    } else if (errno != ENOENT) {
        perror("Failed to open column families file");
        kvarkdb_close(db);
        return -1;
    }

    /* restore sst_paths for each CF from the manifest */
    char manifest_path[KVARKDB_MAX_PATH];
    if (snprintf(manifest_path, sizeof(manifest_path), "%s/%s/%s",
                 db->config.db_path, DATA_DIR, MANIFEST_FILE) >= (int)sizeof(manifest_path)) return -1;

    FILE* mf = fopen(manifest_path, "r");
    if (mf) {
        char cf_name[256], sst_path[KVARKDB_MAX_PATH];
        while (fscanf(mf, "%255s %1023s", cf_name, sst_path) == 2) {
            kvarkdb_column_family_t* cf = find_cf(db, cf_name);
            if (!cf) continue;

            char** new_paths = realloc(cf->sst_paths, (cf->sst_count + 1) * sizeof(char*));
            if (!new_paths) { fclose(mf); kvarkdb_close(db); return -1; }
            cf->sst_paths = new_paths;
            cf->sst_paths[cf->sst_count] = strdup(sst_path);
            if (!cf->sst_paths[cf->sst_count]) { fclose(mf); kvarkdb_close(db); return -1; }
            cf->sst_count++;

            /* derive next_sst_seq from the highest sequence number seen */
            const char* base = strrchr(sst_path, '/');
            base = base ? base + 1 : sst_path;
            uint64_t seq = (uint64_t)strtoull(base, NULL, 10);
            if (seq + 1 > cf->next_sst_seq) cf->next_sst_seq = seq + 1;
        }
        fclose(mf);
    } else if (errno != ENOENT) {
        perror("Failed to open manifest file");
        kvarkdb_close(db);
        return -1;
    }

    /* replay any WAL records written before the last clean close */
    wal_replay(db->config.db_path, wal_replay_cb, db);
    /* clear WAL now that state is restored — start fresh */
    wal_clear(db->config.db_path);

    db->wal = wal_init(db->config.db_path, KVARKDB_WAL_MAX_SIZE);
    if (!db->wal) {
        kvarkdb_close(db);
        return -1;
    }

    return 0;
}

void kvarkdb_close(kvarkdb_t* db) {
    if (!db) return;

    for (size_t i = 0; i < db->column_family_count; i++) {
        kvarkdb_column_family_t* cf = &db->column_families[i];
        if (cf->memtable && cf->memtable->skiplist->size > 0)
            flush_memtable(db, cf);
        free_cf_contents(cf);
    }

    free(db->column_families);
    db->column_families     = NULL;
    db->column_family_count = 0;

    /* all memtables have been flushed to SSTables — WAL is no longer needed */
    if (db->wal) {
        wal_close(db->wal);
        db->wal = NULL;
        wal_clear(db->config.db_path);
    }
}

int kvarddb_create_column_family(kvarkdb_t* db, const char* name) {
    if (!db || !name || name[0] == '\0') return -1;
    if (find_cf(db, name) != NULL) return -1;  /* already exists */

    kvarkdb_column_family_t* new_cfs = realloc(db->column_families,
        (db->column_family_count + 1) * sizeof(kvarkdb_column_family_t));
    if (!new_cfs) return -1;
    db->column_families = new_cfs;

    kvarkdb_column_family_t* cf = &db->column_families[db->column_family_count];
    memset(cf, 0, sizeof(*cf));

    cf->name = strdup(name);
    if (!cf->name) return -1;

    if (memtable_new(&cf->memtable, 12, 0.5f) != 0) {
        free(cf->name); return -1;
    }

    cf->memtable_max_size = db->config.memtable_max_size;
    cf->next_sst_seq      = 1;

    /* create the SSTable directory for this CF */
    char dir[KVARKDB_MAX_PATH];
    if (snprintf(dir, sizeof(dir), "%s/%s/%s/%s",
                 db->config.db_path, DATA_DIR, SSTABLES_DIR, name) >= (int)sizeof(dir)) {
        memtable_destroy(&cf->memtable); free(cf->name); return -1;
    }
    if (make_dir(dir) != 0) {
        memtable_destroy(&cf->memtable); free(cf->name); return -1;
    }

    db->column_family_count++;
    persist_column_families(db);
    return 0;
}

int kvarddb_drop_column_family(kvarkdb_t* db, const char* name) {
    if (!db || !name) return -1;

    size_t idx = db->column_family_count;  /* sentinel = not found */
    for (size_t i = 0; i < db->column_family_count; i++) {
        if (strcmp(db->column_families[i].name, name) == 0) { idx = i; break; }
    }
    if (idx == db->column_family_count) return -1;

    kvarkdb_column_family_t* cf = &db->column_families[idx];
    if (cf->memtable && cf->memtable->skiplist->size > 0)
        flush_memtable(db, cf);
    free_cf_contents(cf);

    size_t remaining = db->column_family_count - idx - 1;
    if (remaining > 0)
        memmove(&db->column_families[idx], &db->column_families[idx + 1],
                remaining * sizeof(kvarkdb_column_family_t));
    db->column_family_count--;

    if (db->column_family_count == 0) {
        free(db->column_families);
        db->column_families = NULL;
    } else {
        db->column_families = realloc(db->column_families,
            db->column_family_count * sizeof(kvarkdb_column_family_t));
    }

    persist_column_families(db);
    return 0;
}

int kvarkdb_put(kvarkdb_t* db, const char* column_family, const char* key, const char* value) {
    if (!db || !column_family || !key || !value) return -1;

    kvarkdb_column_family_t* cf = find_cf(db, column_family);
    if (!cf) return -1;

    if (db->wal) {
        size_t key_len = strlen(key), val_len = strlen(value);
        if (!wal_write_record(db->wal, WAL_OP_PUT, column_family,
                              (const uint8_t*)key,   (uint32_t)key_len,
                              (const uint8_t*)value, (uint32_t)val_len))
            return -1;
    }

    if (memtable_put(cf->memtable,
                     (const uint8_t*)key,   strlen(key),
                     (uint8_t*)value,        strlen(value), 0) != 0) return -1;

    if (memtable_needs_flush(cf->memtable, cf->memtable_max_size))
        return flush_memtable(db, cf);

    return 0;
}

/*
 * kvarkdb_get — checks the memtable first, then walks SSTables newest to oldest.
 * always returns a heap-allocated null-terminated string — caller must free().
 * returns NULL if the key is not found or has been deleted (tombstone).
 */
char* kvarkdb_get(kvarkdb_t* db, const char* column_family, const char* key) {
    if (!db || !column_family || !key) return NULL;

    kvarkdb_column_family_t* cf = find_cf(db, column_family);
    if (!cf) return NULL;

    size_t key_len = strlen(key);

    /* check memtable */
    uint8_t*  val          = NULL;
    size_t*   val_size_ptr = NULL;
    uint8_t   mt_flags     = 0;
    if (memtable_get(cf->memtable, (const uint8_t*)key, key_len,
                     &val, &val_size_ptr, &mt_flags) == 0) {
        if (mt_flags & SKIPLIST_FLAG_DELETED) return NULL;
        char* result = malloc(*val_size_ptr + 1);
        if (!result) return NULL;
        memcpy(result, val, *val_size_ptr);
        result[*val_size_ptr] = '\0';
        return result;
    }

    /* walk SSTables newest → oldest; first tombstone also means deleted */
    for (int i = (int)cf->sst_count - 1; i >= 0; i--) {
        uint8_t* sst_val      = NULL;
        size_t   sst_val_size = 0;
        uint8_t  sst_flags    = 0;
        if (sstable_get(cf->sst_paths[i], (const uint8_t*)key, key_len,
                        &sst_val, &sst_val_size, &sst_flags) == 0) {
            if (sst_flags & SKIPLIST_FLAG_DELETED) { free(sst_val); return NULL; }
            char* result = realloc(sst_val, sst_val_size + 1);
            if (!result) { free(sst_val); return NULL; }
            result[sst_val_size] = '\0';
            return result;
        }
    }

    return NULL;
}

int kvarkdb_delete(kvarkdb_t* db, const char* column_family, const char* key) {
    if (!db || !column_family || !key) return -1;

    kvarkdb_column_family_t* cf = find_cf(db, column_family);
    if (!cf) return -1;

    if (db->wal) {
        size_t key_len = strlen(key);
        if (!wal_write_record(db->wal, WAL_OP_DELETE, column_family,
                              (const uint8_t*)key, (uint32_t)key_len,
                              NULL, 0))
            return -1;
    }

    /* Write a flags-based tombstone (SKIPLIST_FLAG_DELETED) so that keys
     * flushed to SSTable are correctly suppressed on the read path.
     * The tombstone node stays in the skiplist and is flushed to SSTable
     * naturally; compaction will eventually remove it. */
    return memtable_delete(cf->memtable, (const uint8_t*)key, strlen(key));
}
