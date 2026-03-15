# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**kvarkDB** is a C11 key-value database inspired by [tidesdb](https://github.com/tidesdb/tidesdb), implementing an LSM-tree architecture with column family support. Unix-like systems only (macOS, Linux).

## Build Commands

```bash
# Full clean build
./build_project.sh
# or manually:
rm -rf build && cmake -S . -B build && cmake --build build

# Incremental rebuild
./rebuild_project.sh

# Run all tests via CTest
ctest --test-dir build

# Run individual test suites
./build/wal_tests
./build/compress_tests
./build/bloomfilter_tests
./build/skiplist_tests
./build/memtable_tests
./build/sstable_tests
./build/kvarkdb_tests
./build/compaction_tests
./build/wal_integration_tests

# Format code before committing (runs clang-format on all .c/.h files)
./code_formatter.sh
```

**Compiler flags:** `-Wall -Wextra -Werror -pedantic` — all warnings are errors.

**Dependencies:** `lz4`, `zstd`, `snappy`, `pthread`. On Linux, resolved via `pkg-config`. On macOS, brew paths are hardcoded in `CMakeLists.txt` (`/opt/homebrew/opt/`).

## Architecture

### Data Flow
1. Writes go to WAL (durability) and the in-memory skiplist (memtable)
2. When memtable reaches threshold → flush to SSTable on disk
3. Background compaction manages multi-level LSM compaction
4. Reads check memtable first, then SSTables (using bloom filters to skip irrelevant files)

### Core Components

| File | Role |
|------|------|
| `src/kvarkdb.{h,c}` | Top-level DB lifecycle, column family management, put/get/delete, flush trigger |
| `src/memtable.{h,c}` | Thin wrapper over the skiplist; tracks `size_bytes` and `immutable` flag |
| `src/skiplist.{h,c}` | In-memory ordered structure; supports TTL, flags (tombstones), and a bidirectional cursor |
| `src/sstable.{h,c}` | Flush memtable → sorted immutable file; binary-search index + bloom filter per file |
| `src/wal.{h,c}` | Write-ahead log with automatic rotation at configurable max size |
| `src/bloomfilter.{h,c}` | Probabilistic membership filter using FNV hash |
| `src/compress.{h,c}` | Unified wrapper over Snappy, LZ4, ZSTD |
| `src/fnv_hash.{h,c}` | FNV hash used by the bloom filter |
| `src/err.h` | Error code type (`kvarkdb_err_t`) — not yet used by the public API |

### Runtime Directory Layout
```
data/
├── wal/       # Write-ahead log files
├── sstables/  # Flushed sorted string tables (planned)
└── meta/      # Column family registry and manifest
```

### WAL Record Format

Each WAL `.log` file is a sequence of binary records (little-endian fixed-width integers):
```
op_type     (1B)           WAL_OP_PUT (0x01) or WAL_OP_DELETE (0x02)
cf_name_len (2B)           column-family name length
cf_name     (cf_name_len B)
key_size    (4B)
key         (key_size B)
value_size  (4B)           0 for DELETE
value       (value_size B) absent for DELETE
```
The structured API (`wal_write_record`, `wal_replay`, `wal_clear`) handles serialization. `wal_replay` iterates all `.log` files in sequence order and calls a callback per record; truncated/corrupt records silently stop replay for that file.

### SSTable File Format

Each `.sst` file has four sections (documented in `src/sstable.h`):
```
[ data block  ]  key_size(4B) | key | flags(1B) | value_size(4B) | value  (per entry)
[ index block ]  key_size(4B) | key | data_offset(8B)             (sorted, binary-searchable)
[ bloom block ]  raw bit_array bytes
[ footer      ]  index_offset | index_size | bloom_offset | bloom_size | bloom_num_hashes | num_entries  (8B each)
```
`flags` bit `0x01` = `SKIPLIST_FLAG_DELETED` (tombstone; `value_size` is 0).

### Tombstone Design

Deletes write a **flags-based tombstone** (`SKIPLIST_FLAG_DELETED` set in `skiplist_node_t.flags`) rather than a sentinel value in the value bytes. This means any user value — including one that looks like a sentinel — is stored correctly. Tombstone nodes flush to SSTable naturally; compaction will eventually remove them.

### Key API Contracts

- `kvarkdb_t` is **caller-allocated** on the stack. Fill `db.config` fields then call `kvarkdb_open(&db)`.
- `kvarkdb_get` returns a **heap-allocated** null-terminated string — caller must `free()` it. Returns `NULL` for not-found or deleted keys.
- All operations return `0` on success, `-1` on error.
- Note: the column-family functions have a typo in the name — `kvarddb_create_column_family` / `kvarddb_drop_column_family` (missing `k`). This is intentional historical naming; do not change it without updating all call sites.

### Test Conventions
- Test files use `__tests.c` suffix (e.g., `wal__tests.c`)
- Shared test utilities are in `test/test_macros.h`
- Each component has its own test executable built by CMake

## Code Conventions

- Header guards: use `#pragma once`
- Type names: `_t` suffix
- All source formatted with `clang-format` (run `./code_formatter.sh` before PRs)
- C11 standard throughout

## Implementation Status

- [x] WAL with rotation
- [x] Compression (Snappy, LZ4, ZSTD)
- [x] Bloom filter
- [x] SkipList with bidirectional cursor and TTL
- [x] Memtable (skiplist wrapper with size tracking and immutable flag)
- [x] Memtable → SSTable flush (with index block + bloom filter)
- [x] SSTable point-lookup (`sstable_get` with bloom filter + binary search)
- [x] Column families (create, drop, persist/reload across reopen)
- [x] Full CRUD (`put`, `get`, `delete` with tombstone suppression across SSTable levels)
- [x] Compaction (full/flat: merge all SSTables per CF into one, drop tombstones)
- [x] WAL integration with memtable writes (crash recovery via `wal_replay` on open, `wal_clear` on clean close)
- [x] REPL (`src/main.c` + `src/repl.c`; binary: `./build/kvarkdb_repl [--db <path>] [--memtable-size <bytes>]`)
