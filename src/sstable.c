#include "sstable.h"
#include "bloomfilter.h"
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
        if (skiplist_cursor_get(cursor, &key, &key_size, &value, &value_size) != 0) break;

        if (key_size > MAX_KEY_SIZE || value_size > MAX_VALUE_SIZE) goto err;

        index[i].offset   = (uint64_t)ftell(f);
        index[i].key_size = (uint32_t)key_size;
        index[i].key      = malloc(key_size);
        if (!index[i].key) goto err;
        memcpy(index[i].key, key, key_size);

        uint32_t ks = (uint32_t)key_size;
        uint32_t vs = (uint32_t)value_size;
        if (fwrite(&ks,    sizeof(uint32_t), 1, f) != 1) goto err;
        if (fwrite(key,    1, key_size,         f) != key_size) goto err;
        if (fwrite(&vs,    sizeof(uint32_t), 1, f) != 1) goto err;
        if (fwrite(value,  1, value_size,       f) != value_size) goto err;

        bloom_add(bf, (const char*)key);
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

int sstable_get(const char* path, const uint8_t* key, size_t key_size,
                uint8_t** value, size_t* value_size) {
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

    /* seek to data entry, skip key, read value */
    if (fseek(f, (long)data_offset, SEEK_SET) != 0) { fclose(f); return -1; }
    uint32_t stored_ks;
    if (fread(&stored_ks, sizeof(uint32_t), 1, f) != 1) { fclose(f); return -1; }
    if (fseek(f, stored_ks, SEEK_CUR) != 0) { fclose(f); return -1; }
    uint32_t stored_vs;
    if (fread(&stored_vs, sizeof(uint32_t), 1, f) != 1) { fclose(f); return -1; }

    *value = malloc(stored_vs);
    if (!*value) { fclose(f); return -1; }
    if (fread(*value, 1, stored_vs, f) != stored_vs) { free(*value); fclose(f); return -1; }

    *value_size = stored_vs;
    fclose(f);
    return 0;
}
