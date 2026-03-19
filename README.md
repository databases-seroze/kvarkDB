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

## Further Reading

[@dborchard](https://github.com/dborchard) has a great collection of educational database projects worth exploring:

- [mini-lsm](https://github.com/skyzh/mini-lsm) — Structured course for building an LSM-Tree storage engine in Rust (memtables, SSTables, WAL, compaction, MVCC)
- [tidesdb](https://github.com/tidesdb/tidesdb) — The primary inspiration for kvarkDB
- [lsm-tree](https://github.com/dborchard/lsm-tree) — LSM Tree demo (directly relevant to kvarkDB's architecture)
- [cometkv](https://github.com/dborchard/cometkv) — Comparing different memtable implementations
- [tiny-txn](https://github.com/dborchard/tiny-txn) — Serializable Snapshot Isolation transactions
- [isolation_levels](https://github.com/dborchard/isolation_levels) — Database isolation levels implemented in Go
- [colexec-db](https://github.com/dborchard/colexec-db) — Educational vectorized execution engine
- [tiny-db](https://github.com/dborchard/tiny-db) — Query engine + storage engine using Calcite and ANTLR
- [awesome-dbdev](https://github.com/dborchard/awesome-dbdev) — Curated materials on database development

### Research

[Niv Dayan](https://www.nivdayan.net/) (University of Toronto) is a leading researcher on LSM-tree optimization. Notable papers:

- **Monkey** (SIGMOD 2017) — Optimal bloom filter allocation across LSM levels
- **Dostoevsky** (SIGMOD 2018) — Better space-time trade-offs via adaptive merging
- **Chucky** (SIGMOD 2021) — Succinct cuckoo filter for LSM-trees
- **Spooky** (VLDB 2022) — Correct compaction granularity for LSM-trees
- **KV-Tandem** (2024) — Modular approach to high-speed LSM storage engines

### Articles

- [Transaction Isolation in Postgres, Explained](https://www.thenile.dev/blog/transaction-isolation-postgres) — Covers SQL92 isolation levels, MVCC, and real-world concurrency tradeoffs (relevant to kvarkDB's planned MVCC support)

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
