#ifndef KVARKDB_WAL_H
#define KVARKDB_WAL_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>

typedef struct {
    char* dir_path; //path to wal directory
    uint64_t max_size;  // max size before rotation (eg: 1 MB)
    uint64_t file_name; // current file sequence number
    uint64_t file_num; // file number starts with 1
    FILE* current_file; // current active file handle
    uint64_t current_size; // current size
} WAL;

// Initialize the WAL module
WAL* wal_init(const char* db_path, uint64_t max_size);

// write entry to WAL (auto rotates if needed)
bool wal_write(WAL* wal, const char* data, size_t data_len);

// close current WAL and open next
bool wal_rotate(WAL* wal);

// get path to current WAL file
char* wal_current_path(const WAL* wal);

void wal_close(WAL* wal);
#endif
