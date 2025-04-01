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
