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

# Run individual test suites
./build/wal_tests
./build/compress_tests
./build/bloomfilter_tests

# Format code before committing (runs clang-format on all .c/.h files)
./code_formatter.sh
```

**Compiler flags:** `-Wall -Wextra -Werror -pedantic` — all warnings are errors.

**Dependencies:** `lz4`, `zstd`, `snappy`, `pthread`. On macOS, brew paths are hardcoded in `CMakeLists.txt` (`/opt/homebrew/opt/`).

## Architecture

### Data Flow
1. Writes go to WAL (durability) and the in-memory skiplist (memtable)
2. When memtable reaches threshold → flush to SSTable on disk
3. Background compaction manages multi-level LSM compaction
4. Reads check memtable first, then SSTables (using bloom filters to skip irrelevant files)

### Core Components

| File | Role |
|------|------|
| `src/wal.{h,c}` | Write-ahead log with automatic rotation at configurable max size |
| `src/skiplist.{h,c}` | In-memory ordered structure (memtable); supports TTL per node |
| `src/bloomfilter.{h,c}` | Probabilistic membership filter using FNV hash |
| `src/compress.{h,c}` | Unified wrapper over Snappy, LZ4, ZSTD |
| `src/kvarkdb.{h,c}` | Top-level DB lifecycle and column family management |
| `src/err.h` | Error code definitions |
| `src/fnv_hash.{h,c}` | FNV hash used by the bloom filter |

### Runtime Directory Layout
```
data/
├── wal/       # Write-ahead log files
├── sstables/  # Flushed sorted string tables (planned)
└── meta/      # Column family registry and manifest
```

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
- [ ] SkipList + cursor (in progress)
- [ ] Memtable → SSTable flush
- [ ] Column families
- [ ] Full CRUD operations
- [ ] REPL
