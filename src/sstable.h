#ifndef __KVARKDB_SSTABLE_H__
#define __KVARKDB_SSTABLE_H__

#include <stdint.h>
#include <stddef.h>
#include "memtable.h"

/* key and value sizes are serialized as uint32_t — hard limits */
#define SSTABLE_MAX_KEY_SIZE   UINT32_MAX
#define SSTABLE_MAX_VALUE_SIZE UINT32_MAX

/*
 * SSTable file layout:
 *
 * [ data block ]
 *   for each entry: key_size(4B) | key | flags(1B) | value_size(4B) | value
 *   flags bit 0x01 = SKIPLIST_FLAG_DELETED (tombstone, value_size is 0)
 *
 * [ index block ]
 *   for each entry: key_size(4B) | key | data_offset(8B)
 *   entries are in sorted key order, enabling binary search
 *
 * [ bloom block ]
 *   raw bit_array bytes of the bloom filter
 *
 * [ footer - 48 bytes ]
 *   index_offset(8B) | index_size(8B) | bloom_offset(8B)
 *   bloom_size(8B)   | bloom_num_hashes(8B) | num_entries(8B)
 */

/*
 * sstable_write
 * flushes a memtable to a sorted, immutable SSTable file.
 * the memtable is iterated in key order via the cursor.
 * @param memtable  source memtable (must be non-empty)
 * @param path      destination file path
 * @return 0 on success, -1 on failure
 */
int sstable_write(memtable_t* memtable, const char* path);

/*
 * sstable_get
 * looks up a key in an SSTable file.
 * checks the bloom filter first to avoid unnecessary disk reads.
 * uses binary search on the in-memory index.
 * @param path        SSTable file path
 * @param key         key to look up
 * @param key_size    size of key
 * @param value       output: heap-allocated value buffer — caller must free (NULL for tombstones)
 * @param value_size  output: size of value (0 for tombstones)
 * @param flags       output: entry flags (SKIPLIST_FLAG_DELETED if tombstone)
 * @return 0 if found (including tombstones), -1 if not found
 */
int sstable_get(const char* path, const uint8_t* key, size_t key_size,
                uint8_t** value, size_t* value_size, uint8_t* flags);

#endif
