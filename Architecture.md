Vocabulary:
- column family is nothing but a column in a SQL table

Metadata management:

- Everything will be saved under data directory in the current folder
- Column family registry will be stored in meta/column_families
  - simple text file listing all column families
  - one column family per line
- Manifest file consisting of information like mapping of SSTables to column families is stored in meta/manifest

Startup flow:
1. we check if db's data dir exists if not we create one
2. read the meta/column_families to load all the column_families
3. for each column_family
  1. initialize a new skip_list for the memtable
  2.
4. initialize the background compaction thread
5. when a memtable reaches it's size threshold
  1. create a new sstable from memtable
  2. write the sstable to the appropriate column family directory
  3. update the manifest file to include the directories
  4. clear the memtable and create a new empty one

---

## Compaction Design History

### v1 — Full/Flat Compaction (current)

**Implemented:** initial compaction milestone.

The first version uses the simplest possible strategy: there are no levels.
Each column family holds a flat array of SSTables, ordered oldest to newest:

```
cf->sst_paths = [ sst1, sst2, sst3, sst4 ]
                  oldest             newest
```

**Trigger:** after every memtable flush, if `sst_count >= compaction_threshold`
(configurable, default 4), compact immediately on the same thread.

**Algorithm (N-way sort-merge):**
1. Read every entry from every SSTable into one flat in-memory array
2. Sort by key ascending; for duplicate keys, newest SSTable wins
3. Keep only the first (newest) occurrence of each key
4. Drop tombstones — safe because ALL SSTables are being merged, so no
   older data exists that a tombstone still needs to suppress
5. Write survivors to a new SSTable file
6. Delete the old SSTable files and rewrite the manifest

**Result:** N SSTables collapse to 1.

**Why start here:**
- Simple to reason about and test
- Correct — tombstone handling, duplicate resolution, and manifest updates
  are all straightforward when scope is always "everything"
- Good foundation to layer leveled compaction on top of

**Known limitations:**
- **Write amplification** — every compaction rewrites the entire dataset for
  that column family, even if only a few keys changed
- **No partial compaction** — you can't compact just a subset of SSTables;
  it's all-or-nothing
- **Synchronous** — compaction blocks writes on the same thread; no
  background compaction thread yet
- **Single output file** — after compaction there is always exactly 0 or 1
  SSTable per CF; a very large dataset would produce one very large file

---

### v2 — Leveled Compaction (planned)

The standard approach used by LevelDB and RocksDB. Each column family has
multiple levels (L0, L1, L2, ...) where each level is ~10x larger than the
previous. Compaction picks one file from level N and merges it with the
overlapping key-range files in level N+1, producing a new sorted run at N+1.

Key improvements over v1:
- **Bounded read amplification** — at most one file per level needs to be
  checked for a key lookup (L0 is the exception; files there can overlap)
- **Controlled write amplification** — only a small subset of keys are
  rewritten per compaction, not the whole dataset
- **Incremental** — compaction can run continuously in a background thread
  without stalling writes for long
- **Tombstone handling** — tombstones are only dropped when compacting into
  the bottom level (where no older data can exist)

The manifest format and `cf->sst_paths` array would need to evolve into a
per-level structure to support this.
