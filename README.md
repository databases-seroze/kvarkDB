# kvarkDB

This is mainly inspired from [tidesdb](https://github.com/tidesdb/tidesdb) from the author Alex Padula
But i'll try to add some experimental ideas.

Platform support:
- Only targeted for unix like systems (eg: Mac, Ubuntu). If you want windows support please ask by raising a PR.

How to run:

// delete existing build dir and create a new make file
// then run the cmake to actuall build the project
% rm -rf build && cmake -S . -B build
% cmake --build build
Note: cmake --build build automatically takes care of rebuilding only the relevant files that have changed
      post last run

How to run tests:
% ./build/*_tests

Milestones:

- [X] Implement WAL
- [X] Implement Compression
- [X] Implement BloomFilter
- [] Implement SkipList + cursor
- [] Implement memtable & sstable
- [] Implement Column families
- [] Write a minimal key-value db
- [] Support REPL
- [] Add Go, python bindings

Features:

- [] LSM Based with levelled compaction (for now only has single level)
- [] Transaction support
- [X] WAL
- [X] Compression support

Dependencies
- install zstd4, snappy, lz4 libraries
