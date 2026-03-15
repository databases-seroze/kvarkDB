#ifndef __KVARKDB_WAL_H__
#define __KVARKDB_WAL_H__

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    char*    dir_path;     /* path to wal directory */
    uint64_t max_size;     /* max file size before rotation */
    uint64_t file_name;    /* current file sequence number */
    uint64_t file_num;     /* file number starts with 1 */
    FILE*    current_file; /* current active file handle */
    uint64_t current_size; /* bytes written to current file */
} WAL;

/* Initialize the WAL; creates db_path/wal/ if needed */
WAL* wal_init(const char* db_path, uint64_t max_size);

/* Write raw bytes to WAL (auto-rotates if needed) */
bool wal_write(WAL* wal, const char* data, size_t data_len);

/* Close current file and open the next sequence number */
bool wal_rotate(WAL* wal);

/* Return a heap-allocated path to the current WAL file (caller must free) */
char* wal_current_path(const WAL* wal);

/* Close the WAL file handle and free the WAL struct */
void wal_close(WAL* wal);

/* ------------------------------------------------------------------ */
/* Structured record API                                                */

#define WAL_OP_PUT    0x01
#define WAL_OP_DELETE 0x02

/*
 * WAL record binary format (little-endian fixed-width integers):
 *
 *   op_type   (1B)              WAL_OP_PUT or WAL_OP_DELETE
 *   cf_name_len (2B)            length of column-family name in bytes
 *   cf_name   (cf_name_len B)   column-family name (not null-terminated)
 *   key_size  (4B)              key length
 *   key       (key_size B)
 *   value_size (4B)             0 for DELETE
 *   value     (value_size B)    absent for DELETE
 */

typedef struct {
    uint8_t        op_type;
    const char*    cf_name;    /* null-terminated for convenience */
    const uint8_t* key;
    uint32_t       key_size;
    const uint8_t* value;      /* NULL for DELETE */
    uint32_t       value_size; /* 0 for DELETE */
} wal_record_t;

/* Serialize and write one structured record */
bool wal_write_record(WAL* wal, uint8_t op_type,
                      const char* cf_name,
                      const uint8_t* key,   uint32_t key_size,
                      const uint8_t* value, uint32_t value_size);

/*
 * Replay all WAL files under db_path/wal/ in sequence order.
 * fn is called once per record; if fn returns non-zero replay stops.
 * Returns the number of records replayed, or -1 on a read error.
 */
typedef int (*wal_replay_fn)(const wal_record_t* record, void* ctx);
int wal_replay(const char* db_path, wal_replay_fn fn, void* ctx);

/* Delete all WAL log files under db_path/wal/ */
void wal_clear(const char* db_path);

#endif
