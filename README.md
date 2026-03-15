# kvarkDB

This is mainly inspired from [tidesdb](https://github.com/tidesdb/tidesdb) from the author Alex Padula
But i'll try to add some experimental ideas.

## Platform support
Only targeted for unix like systems (eg: Mac, Ubuntu). If you want windows support please ask by raising a PR.

## Dependencies

**macOS**
```bash
brew install lz4 zstd snappy
```

**Ubuntu / Debian**
```bash
sudo apt install liblz4-dev libzstd-dev libsnappy-dev
```

**Fedora / RHEL**
```bash
sudo dnf install lz4-devel libzstd-devel snappy-devel
```

## How to run

```bash
# delete existing build dir, configure, and build
rm -rf build && cmake -S . -B build
cmake --build build
```

> `cmake --build build` automatically rebuilds only files that changed since the last run.

## How to run tests

```bash
./build/*_tests
```

## Milestones

- [x] Implement WAL
- [x] Implement Compression
- [x] Implement BloomFilter
- [x] Implement SkipList
- [x] Implement cursor
- [x] Implement memtable
- [x] Implement sstable
- [x] Implement Column families
- [x] Write a minimal key-value db
- [x] Support REPL
- [ ] Add Python bindings

## Features

- [ ] LSM based with levelled compaction (for now only has single level)
- [ ] Transaction support
- [x] WAL
- [x] Compression support (LZ4, ZSTD, Snappy)
