# kvarkDB

This is mainly inspired from [tidesdb](https://github.com/tidesdb/tidesdb)
But i'll try to add some experimental ideas.

Design Goals:
- Only targeted for unix like systems (eg: Mac, Ubuntu)

Milestones:

[] Use LSM based storage engine (memtable(skiplist) + sstable(bloomfilter+disk ds))
[] Write a minimal key-value db
[] Support REPL

Features:

[] LSM Based with levelled compaction
[] Transaction support
[] WAL
[] Compression support
