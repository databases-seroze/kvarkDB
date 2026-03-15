#include "sstable.h"
#include "bloomfilter.h"
#include "skiplist.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

/* key and value sizes are stored as uint32_t on disk */
#define MAX_KEY_SIZE   UINT32_MAX
#define MAX_VALUE_SIZE UINT32_MAX

#define BLOOM_BITS_PER_KEY 10
#define BLOOM_NUM_HASHES   7
#define FOOTER_SIZE        (6 * sizeof(uint64_t))  /* 48 bytes */

typedef struct {
    uint64_t index_offset;
    uint64_t index_size;
    uint64_t bloom_offset;
    uint64_t bloom_size;
    uint64_t bloom_num_hashes;
    uint64_t num_entries;
} sstable_footer_t;

/* in-memory index entry used during both write and read */
typedef struct {
    uint8_t*  key;
    uint32_t  key_size;
    uint64_t  offset;  /* byte offset into data block */
} index_entry_t;

static void free_index(index_entry_t* index, size_t count) {
    for (size_t i = 0; i < count; i++) free(index[i].key);
    free(index);
}

/* raw entry used during compaction merge */
typedef struct {
    uint8_t* key;
    uint32_t key_size;
    uint8_t* value;
    uint32_t value_size;
    uint8_t  flags;
    size_t   sst_idx;  /* 0 = oldest, higher = newer; used to pick winner on dup key */
} raw_entry_t;

static void free_raw_entries(raw_entry_t* entries, size_t count) {
    if (!entries) return;
    for (size_t i = 0; i < count; i++) {
        free(entries[i].key);
        free(entries[i].value);
    }
    free(entries);
}

/* qsort comparator: sort by key ascending, then sst_idx descending (newest first) */
static int entry_compare(const void* a, const void* b) {
    const raw_entry_t* ea = (const raw_entry_t*)a;
    const raw_entry_t* eb = (const raw_entry_t*)b;
    size_t min_len = ea->key_size < eb->key_size ? ea->key_size : eb->key_size;
    int cmp = memcmp(ea->key, eb->key, min_len);
    if (cmp != 0) return cmp;
    if (ea->key_size != eb->key_size)
        return (ea->key_size > eb->key_size) ? 1 : -1;
    /* same key: newest SSTable first so it becomes the kept entry */
    if (ea->sst_idx > eb->sst_idx) return -1;
    if (ea->sst_idx < eb->sst_idx) return  1;
    return 0;
}

/*
 * sstable_read_all — loads every entry from an SSTable file into a heap array.
 * sst_idx identifies which SSTable (for dup resolution during merge).
 * on success sets *out_entries and *out_count; caller owns the array.
 */
static int sstable_read_all(const char* path, raw_entry_t** out_entries,
                            size_t* out_count, size_t sst_idx) {
    FILE* f = NULL;
    index_entry_t* idx = NULL;
    raw_entry_t* entries = NULL;
    size_t n = 0;

    f = fopen(path, "rb");
    if (!f) return -1;

    if (fseek(f, -(long)sizeof(sstable_footer_t), SEEK_END) != 0) goto fail;
    sstable_footer_t footer;
    if (fread(&footer, sizeof(footer), 1, f) != 1) goto fail;
    n = (size_t)footer.num_entries;

    if (n == 0) {
        fclose(f);
        *out_entries = NULL;
        *out_count = 0;
        return 0;
    }

    /* load index block */
    if (fseek(f, (long)footer.index_offset, SEEK_SET) != 0) goto fail;
    idx = calloc(n, sizeof(index_entry_t));
    if (!idx) goto fail;

    for (size_t i = 0; i < n; i++) {
        uint32_t ks;
        if (fread(&ks, sizeof(uint32_t), 1, f) != 1)  { free_index(idx, i);     idx = NULL; goto fail; }
        idx[i].key_size = ks;
        idx[i].key = malloc(ks);
        if (!idx[i].key)                               { free_index(idx, i);     idx = NULL; goto fail; }
        if (fread(idx[i].key, 1, ks, f) != ks)        { free_index(idx, i + 1); idx = NULL; goto fail; }
        if (fread(&idx[i].offset, sizeof(uint64_t), 1, f) != 1) {
            free_index(idx, i + 1); idx = NULL; goto fail;
        }
    }

    /* read data entries in index order (sorted) */
    entries = calloc(n, sizeof(raw_entry_t));   /* all pointers zeroed */
    if (!entries) goto fail;

    for (size_t i = 0; i < n; i++) {
        if (fseek(f, (long)idx[i].offset, SEEK_SET) != 0) goto fail;

        uint32_t ks;
        if (fread(&ks, sizeof(uint32_t), 1, f) != 1) goto fail;
        entries[i].key_size = ks;
        entries[i].key = malloc(ks);
        if (!entries[i].key) goto fail;
        if (fread(entries[i].key, 1, ks, f) != ks)   goto fail;

        if (fread(&entries[i].flags, sizeof(uint8_t), 1, f) != 1) goto fail;

        uint32_t vs;
        if (fread(&vs, sizeof(uint32_t), 1, f) != 1) goto fail;
        entries[i].value_size = vs;
        if (vs > 0) {
            entries[i].value = malloc(vs);
            if (!entries[i].value) goto fail;
            if (fread(entries[i].value, 1, vs, f) != vs) goto fail;
        }
        entries[i].sst_idx = sst_idx;
    }

    free_index(idx, n);
    fclose(f);
    *out_entries = entries;
    *out_count = n;
    return 0;

fail:
    if (idx)     free_index(idx, n);
    if (entries) free_raw_entries(entries, n);  /* safe: array was calloc'd */
    if (f)       fclose(f);
    return -1;
}

/*
 * sstable_write_entries — writes a sorted raw_entry_t array to an SSTable file.
 * entries must already be in ascending key order.
 * this is essentially sstable_write() but from a raw array rather than a memtable.
 */
static int sstable_write_entries(const raw_entry_t* entries, size_t count,
                                 const char* path) {
    if (count == 0) return -1;

    BloomFilter* bf = bloom_create(count * BLOOM_BITS_PER_KEY, BLOOM_NUM_HASHES);
    if (!bf) return -1;

    index_entry_t* idx = calloc(count, sizeof(index_entry_t));
    if (!idx) { bloom_destroy(bf); return -1; }

    FILE* f = fopen(path, "wb");
    if (!f) { free(idx); bloom_destroy(bf); return -1; }

    /* write data block, build index and bloom filter */
    for (size_t i = 0; i < count; i++) {
        idx[i].offset   = (uint64_t)ftell(f);
        idx[i].key_size = entries[i].key_size;
        idx[i].key      = malloc(entries[i].key_size);
        if (!idx[i].key) goto fail;
        memcpy(idx[i].key, entries[i].key, entries[i].key_size);

        uint32_t ks = entries[i].key_size;
        uint32_t vs = entries[i].value_size;
        if (fwrite(&ks,              sizeof(uint32_t), 1,  f) != 1)  goto fail;
        if (fwrite(entries[i].key,   1,                ks, f) != ks) goto fail;
        if (fwrite(&entries[i].flags,sizeof(uint8_t),  1,  f) != 1)  goto fail;
        if (fwrite(&vs,              sizeof(uint32_t), 1,  f) != 1)  goto fail;
        if (vs > 0 && fwrite(entries[i].value, 1, vs, f) != vs)      goto fail;

        /*
         * bloom_add calls fnv1a_hash(item, strlen(item) + i) for i in
         * [0, num_hashes).  For i > 0 it intentionally reads past the null
         * terminator; allocate enough padding so those reads stay in-bounds.
         */
        {
            char* bloom_key = calloc(entries[i].key_size + 1 + BLOOM_NUM_HASHES, 1);
            if (!bloom_key) goto fail;
            memcpy(bloom_key, entries[i].key, entries[i].key_size);
            bloom_add(bf, bloom_key);
            free(bloom_key);
        }
    }

    /* write index block */
    {
        uint64_t index_offset = (uint64_t)ftell(f);
        for (size_t j = 0; j < count; j++) {
            if (fwrite(&idx[j].key_size, sizeof(uint32_t), 1, f) != 1)             goto fail;
            if (fwrite(idx[j].key, 1, idx[j].key_size, f) != idx[j].key_size)      goto fail;
            if (fwrite(&idx[j].offset, sizeof(uint64_t), 1, f) != 1)               goto fail;
        }
        uint64_t index_size = (uint64_t)ftell(f) - index_offset;

        free_index(idx, count);
        idx = NULL;

        /* write bloom block */
        uint64_t bloom_offset = (uint64_t)ftell(f);
        if (fwrite(bf->bit_array, 1, bf->size, f) != bf->size) goto fail;
        uint64_t bloom_size = (uint64_t)bf->size;
        bloom_destroy(bf);
        bf = NULL;

        /* write footer */
        sstable_footer_t footer = {
            .index_offset     = index_offset,
            .index_size       = index_size,
            .bloom_offset     = bloom_offset,
            .bloom_size       = bloom_size,
            .bloom_num_hashes = BLOOM_NUM_HASHES,
            .num_entries      = (uint64_t)count,
        };
        if (fwrite(&footer, sizeof(footer), 1, f) != 1) goto fail;
    }

    fclose(f);
    return 0;

fail:
    if (idx) free_index(idx, count);
    if (bf)  bloom_destroy(bf);
    fclose(f);
    remove(path);
    return -1;
}

int sstable_write(memtable_t* memtable, const char* path) {
    if (memtable == NULL || path == NULL) return -1;

    size_t num_entries = memtable->skiplist->size;
    if (num_entries == 0) return -1;

    BloomFilter* bf = bloom_create(num_entries * BLOOM_BITS_PER_KEY, BLOOM_NUM_HASHES);
    if (!bf) return -1;

    index_entry_t* index = calloc(num_entries, sizeof(index_entry_t));
    if (!index) { bloom_destroy(bf); return -1; }

    FILE* f = fopen(path, "wb");
    if (!f) { free(index); bloom_destroy(bf); return -1; }

    skiplist_cursor_t* cursor = NULL;
    if (skiplist_cursor_init(memtable->skiplist, &cursor) != 0) {
        fclose(f); free(index); bloom_destroy(bf); return -1;
    }

    /* write data block, build index and bloom filter */
    size_t i = 0;
    do {
        uint8_t* key; size_t key_size;
        uint8_t* value; size_t value_size;
        uint8_t  node_flags;
        if (skiplist_cursor_get(cursor, &key, &key_size, &value, &value_size,
                                &node_flags) != 0) break;

        if (key_size > MAX_KEY_SIZE || value_size > MAX_VALUE_SIZE) goto err;

        index[i].offset   = (uint64_t)ftell(f);
        index[i].key_size = (uint32_t)key_size;
        index[i].key      = malloc(key_size);
        if (!index[i].key) goto err;
        memcpy(index[i].key, key, key_size);

        uint32_t ks = (uint32_t)key_size;
        uint32_t vs = (uint32_t)value_size;
        if (fwrite(&ks,         sizeof(uint32_t), 1,          f) != 1)          goto err;
        if (fwrite(key,         1,                key_size,    f) != key_size)   goto err;
        if (fwrite(&node_flags, sizeof(uint8_t),  1,          f) != 1)          goto err;
        if (fwrite(&vs,         sizeof(uint32_t), 1,          f) != 1)          goto err;
        if (vs > 0 && fwrite(value, 1, value_size, f) != value_size)            goto err;

        /*
         * bloom_add calls fnv1a_hash(item, strlen(item) + i) for i in
         * [0, num_hashes).  For i > 0 it intentionally reads past the null
         * terminator; allocate enough padding so those reads stay in-bounds.
         */
        {
            char* bloom_key = calloc(key_size + 1 + BLOOM_NUM_HASHES, 1);
            if (!bloom_key) goto err;
            memcpy(bloom_key, key, key_size);
            bloom_add(bf, bloom_key);
            free(bloom_key);
        }
        i++;
    } while (skiplist_cursor_next(cursor) == 0);

    skiplist_cursor_destroy(cursor);
    cursor = NULL;

    /* write index block */
    uint64_t index_offset = (uint64_t)ftell(f);
    for (size_t j = 0; j < i; j++) {
        if (fwrite(&index[j].key_size, sizeof(uint32_t), 1, f) != 1) goto err;
        if (fwrite(index[j].key, 1, index[j].key_size,   f) != index[j].key_size) goto err;
        if (fwrite(&index[j].offset, sizeof(uint64_t), 1, f) != 1) goto err;
    }
    uint64_t index_size = (uint64_t)ftell(f) - index_offset;

    free_index(index, i);
    index = NULL;

    /* write bloom filter block */
    uint64_t bloom_offset = (uint64_t)ftell(f);
    if (fwrite(bf->bit_array, 1, bf->size, f) != bf->size) {
        bloom_destroy(bf); fclose(f); return -1;
    }
    uint64_t bloom_size = (uint64_t)bf->size;
    bloom_destroy(bf);
    bf = NULL;

    /* write footer */
    sstable_footer_t footer = {
        .index_offset     = index_offset,
        .index_size       = index_size,
        .bloom_offset     = bloom_offset,
        .bloom_size       = bloom_size,
        .bloom_num_hashes = BLOOM_NUM_HASHES,
        .num_entries      = i,
    };
    if (fwrite(&footer, sizeof(footer), 1, f) != 1) { fclose(f); return -1; }

    fclose(f);
    return 0;

err:
    if (cursor) skiplist_cursor_destroy(cursor);
    if (index)  free_index(index, i);
    if (bf)     bloom_destroy(bf);
    fclose(f);
    return -1;
}

int sstable_merge(const char** paths, size_t count, const char* out_path) {
    if (!paths || count == 0 || !out_path) return -1;

    /* read all entries from all SSTables into one flat array */
    raw_entry_t** per_sst = calloc(count, sizeof(raw_entry_t*));
    size_t*       per_cnt = calloc(count, sizeof(size_t));
    if (!per_sst || !per_cnt) { free(per_sst); free(per_cnt); return -1; }

    size_t total = 0;
    for (size_t i = 0; i < count; i++) {
        if (sstable_read_all(paths[i], &per_sst[i], &per_cnt[i], i) != 0) {
            for (size_t j = 0; j < i; j++) free_raw_entries(per_sst[j], per_cnt[j]);
            free(per_sst); free(per_cnt);
            return -1;
        }
        total += per_cnt[i];
    }

    if (total == 0) {
        free(per_sst); free(per_cnt);
        return 1;
    }

    raw_entry_t* flat = calloc(total, sizeof(raw_entry_t));
    if (!flat) {
        for (size_t i = 0; i < count; i++) free_raw_entries(per_sst[i], per_cnt[i]);
        free(per_sst); free(per_cnt);
        return -1;
    }

    size_t pos = 0;
    for (size_t i = 0; i < count; i++) {
        if (per_cnt[i] > 0) {
            memcpy(flat + pos, per_sst[i], per_cnt[i] * sizeof(raw_entry_t));
            pos += per_cnt[i];
        }
        free(per_sst[i]);   /* free the array shell; entries are now owned by flat */
    }
    free(per_sst);
    free(per_cnt);

    /* sort: key ascending, sst_idx descending for ties (newest entry first per key) */
    qsort(flat, total, sizeof(raw_entry_t), entry_compare);

    /*
     * Deduplicate and drop tombstones in two passes to avoid use-after-free.
     *
     * Pass 1: mark which entries to keep.  We read flat[i-1].key for comparison;
     *         no keys are freed yet so every pointer is still valid.
     *         Entries for the same key are adjacent (sorted); the first occurrence
     *         is the newest (highest sst_idx sorts first).  A tombstone at the
     *         newest position suppresses all older copies of that key.
     *
     * Pass 2: transfer kept entries to `out`; free discarded entries.
     */
    bool* keep = calloc(total, sizeof(bool));
    raw_entry_t* out = calloc(total, sizeof(raw_entry_t));
    if (!keep || !out) {
        free(keep); free(out);
        free_raw_entries(flat, total); free(flat);
        return -1;
    }

    for (size_t i = 0; i < total; i++) {
        bool same_key = (i > 0 &&
                         flat[i].key_size == flat[i - 1].key_size &&
                         memcmp(flat[i].key, flat[i - 1].key, flat[i].key_size) == 0);
        keep[i] = !same_key && !(flat[i].flags & SKIPLIST_FLAG_DELETED);
    }

    size_t out_count = 0;
    for (size_t i = 0; i < total; i++) {
        if (keep[i]) {
            out[out_count++] = flat[i];  /* transfer ownership */
        } else {
            free(flat[i].key);
            free(flat[i].value);
        }
    }
    free(keep);
    free(flat);  /* shell only; entries were moved to out or freed above */

    int rc;
    if (out_count == 0) {
        rc = 1;  /* all entries were tombstones or duplicates */
    } else {
        rc = (sstable_write_entries(out, out_count, out_path) == 0) ? 0 : -1;
    }

    free_raw_entries(out, out_count);   /* also frees the out array itself */
    return rc;
}

int sstable_get(const char* path, const uint8_t* key, size_t key_size,
                uint8_t** value, size_t* value_size, uint8_t* flags) {
    if (path == NULL || key == NULL) return -1;

    FILE* f = fopen(path, "rb");
    if (!f) return -1;

    /* read footer */
    if (fseek(f, -(long)sizeof(sstable_footer_t), SEEK_END) != 0) { fclose(f); return -1; }
    sstable_footer_t footer;
    if (fread(&footer, sizeof(footer), 1, f) != 1) { fclose(f); return -1; }

    /* TODO: bloom_add/bloom_check use strlen() internally so they require
     * null-terminated keys. Skiplist nodes don't store a null terminator,
     * causing hash mismatches (false negatives). The bloom filter API needs
     * a bloom_add_bytes(bf, key, len) variant before this can be used safely.
     * For now the bloom block is written but not checked on read. */
    (void)footer.bloom_offset;
    (void)footer.bloom_size;
    (void)footer.bloom_num_hashes;

    /* load index into memory */
    if (fseek(f, (long)footer.index_offset, SEEK_SET) != 0) { fclose(f); return -1; }
    index_entry_t* index = calloc(footer.num_entries, sizeof(index_entry_t));
    if (!index) { fclose(f); return -1; }

    for (size_t i = 0; i < footer.num_entries; i++) {
        uint32_t ks;
        if (fread(&ks, sizeof(uint32_t), 1, f) != 1) { free_index(index, i); fclose(f); return -1; }
        index[i].key_size = ks;
        index[i].key      = malloc(ks);
        if (!index[i].key) { free_index(index, i); fclose(f); return -1; }
        if (fread(index[i].key, 1, ks, f) != ks) { free_index(index, i + 1); fclose(f); return -1; }
        if (fread(&index[i].offset, sizeof(uint64_t), 1, f) != 1) { free_index(index, i + 1); fclose(f); return -1; }
    }

    /* binary search the index */
    int lo = 0, hi = (int)footer.num_entries - 1, found = -1;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        size_t min_size = index[mid].key_size < key_size ? index[mid].key_size : key_size;
        int cmp = memcmp(index[mid].key, key, min_size);
        if (cmp == 0) cmp = (index[mid].key_size > key_size) - (index[mid].key_size < key_size);

        if      (cmp == 0) { found = mid; break; }
        else if (cmp < 0)  lo = mid + 1;
        else               hi = mid - 1;
    }

    uint64_t data_offset = found != -1 ? index[found].offset : 0;
    free_index(index, footer.num_entries);

    if (found == -1) { fclose(f); return -1; }

    /* seek to data entry: key_size | key | flags(1B) | value_size | value */
    if (fseek(f, (long)data_offset, SEEK_SET) != 0) { fclose(f); return -1; }
    uint32_t stored_ks;
    if (fread(&stored_ks, sizeof(uint32_t), 1, f) != 1) { fclose(f); return -1; }
    if (fseek(f, stored_ks, SEEK_CUR) != 0) { fclose(f); return -1; }

    uint8_t stored_flags;
    if (fread(&stored_flags, sizeof(uint8_t), 1, f) != 1) { fclose(f); return -1; }

    uint32_t stored_vs;
    if (fread(&stored_vs, sizeof(uint32_t), 1, f) != 1) { fclose(f); return -1; }

    if (stored_vs > 0) {
        *value = malloc(stored_vs);
        if (!*value) { fclose(f); return -1; }
        if (fread(*value, 1, stored_vs, f) != stored_vs) { free(*value); fclose(f); return -1; }
    } else {
        *value = NULL;
    }

    *value_size = stored_vs;
    if (flags) *flags = stored_flags;
    fclose(f);
    return 0;
}
