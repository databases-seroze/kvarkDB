# kvarkDB

This is mainly inspired from [tidesdb](https://github.com/tidesdb/tidesdb)
But i'll try to add some experimental ideas.

Platform support:
- Only targeted for unix like systems (eg: Mac, Ubuntu)

How to run:

// delete existing build dir and create a new make file
// then run the cmake to actuall build the project
% rm -rf build && cmake -S . -B build
% cmake --build build

How to run tests:
% ./build/*_tests

Milestones:

- [X] Implement WAL
- [] Implement Column families
- [] Use LSM based storage engine (memtable(skiplist) + sstable(bloomfilter+disk ds))
- [] Write a minimal key-value db
- [] Support REPL

Features:

- [] LSM Based with levelled compaction
- [] Transaction support
- [] WAL
- [] Compression support

How to run:

- run `cmake .` in the directory where CMakeFiles exist
-

Dependencies
- install cmoka (via brew install cmoka if you are on mac)
