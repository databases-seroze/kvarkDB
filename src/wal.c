#include "wal.h"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <dirent.h>
#include <inttypes.h>

#define WAL_FILE_PREFIX "wal_"
#define WAL_FILE_EXT ".log"
// #define WAL_FILE_PATTERN WAL_FILE_PREFIX "%06llu" WAL_FILE_EXT
#define WAL_FILE_PATTERN WAL_FILE_PREFIX "%06" PRIu64 WAL_FILE_EXT


static bool ensure_dir_exists(const char* path) {
    struct stat st = {0};
    if (stat(path, &st) == -1) {
        if (mkdir(path, 0755)) {
            perror("Failed to create WAL directory");
            return false;
        }
    }
    return true;
}

WAL* wal_init(const char* db_path, uint64_t max_size) {
    char wal_dir[1024];
    snprintf(wal_dir, sizeof(wal_dir), "%s/wal", db_path);

    if (!ensure_dir_exists(wal_dir)) {
        return NULL;
    }

    WAL* wal = malloc(sizeof(WAL));
    wal->dir_path = strdup(wal_dir);
    wal->max_size = max_size;
    wal->file_name = 1;
    wal->current_size = 0;
    wal->current_file = NULL;

    // Find the highest existing WAL file number
    DIR* dir = opendir(wal_dir);
    //if dir doesn't exist throw error
    struct dirent* entry;
    uint32_t max_num = 0;
    while ((entry=readdir(dir))!=NULL) {
        if (sscanf(entry->d_name, WAL_FILE_PATTERN, &wal->file_num) == 1) {
            if (wal->file_num > max_num) {
                max_num = wal->file_num;
            }
        }
    }
    closedir(dir);
    wal->file_num = max_num+1;

    // open the wal file
    char path[1024];//filenames can be atmax 1024
    snprintf(path, sizeof(path), "%s/"WAL_FILE_PATTERN, wal->dir_path, wal->file_num);
    wal->current_file = fopen(path, "ab+");//append binary mode
    if (!wal->current_file){
        perror("failed to open WAL file");
        free(wal->dir_path);
        free(wal);
        return NULL;
    }

    // get the current size
    fseek(wal->current_file, 0, SEEK_END);
    wal->current_size = ftell(wal->current_file);
    return wal;
}

bool wal_write(WAL* wal, const char* data, size_t data_len) {
    // check if we need to rotate
    if (wal->current_size + data_len > wal->max_size) {
        if (!wal_rotate(wal)) {
            return false;
        }
    }

    size_t written = fwrite(data, 1, data_len, wal->current_file);
    if (written != data_len) {
        perror("Failed to write to WAL");
        return false;
    }

    fflush(wal->current_file);
    wal->current_size += written;
    return true;
}

bool wal_rotate(WAL* wal) {
    if (wal -> current_file) {
        fclose(wal->current_file);
    }

    wal->file_num++;
    wal->current_size = 0;

    char path[1024];
    snprintf(path, sizeof(path), "%s/" WAL_FILE_PATTERN, wal->dir_path, wal->file_num);
    wal->current_file = fopen(path, "ab+");
    if (!wal->current_file) {
        perror("Failed to rotate WAL file");
        return false;
    }
    return true;
}

void wal_close(WAL* wal) {
    if (wal->current_file) {
        fclose(wal->current_file);
    }

    free(wal->dir_path);
    free(wal);
}

char* wal_current_path(const WAL *wal) {
    char* path = malloc(strlen(wal->dir_path) + 20);  // Enough space
    snprintf(path, strlen(wal->dir_path) + 20, "%s/" WAL_FILE_PATTERN,
            wal->dir_path, wal->file_num);
    return path;
}

/* ------------------------------------------------------------------ */
/* Structured record API                                                */

bool wal_write_record(WAL* wal, uint8_t op_type,
                      const char* cf_name,
                      const uint8_t* key,   uint32_t key_size,
                      const uint8_t* value, uint32_t value_size) {
    uint16_t cf_len = (uint16_t)strlen(cf_name);
    size_t total = 1 + 2 + cf_len + 4 + key_size + 4 + value_size;

    uint8_t* buf = malloc(total);
    if (!buf) return false;

    size_t pos = 0;
    buf[pos++] = op_type;
    memcpy(buf + pos, &cf_len, 2);     pos += 2;
    memcpy(buf + pos, cf_name, cf_len); pos += cf_len;
    memcpy(buf + pos, &key_size, 4);   pos += 4;
    memcpy(buf + pos, key, key_size);  pos += key_size;
    memcpy(buf + pos, &value_size, 4); pos += 4;
    if (value_size > 0) memcpy(buf + pos, value, value_size);

    bool ok = wal_write(wal, (const char*)buf, total);
    free(buf);
    return ok;
}

/* qsort comparator for uint64_t file sequence numbers */
static int cmp_u64(const void* a, const void* b) {
    uint64_t x = *(const uint64_t*)a;
    uint64_t y = *(const uint64_t*)b;
    return (x > y) - (x < y);
}

int wal_replay(const char* db_path, wal_replay_fn fn, void* ctx) {
    char wal_dir[1024];
    snprintf(wal_dir, sizeof(wal_dir), "%s/wal", db_path);

    DIR* dir = opendir(wal_dir);
    if (!dir) return 0;  /* no WAL directory — nothing to replay */

    /* collect all WAL file sequence numbers */
    uint64_t nums[4096];
    size_t   num_count = 0;
    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL && num_count < 4096) {
        uint64_t n;
        if (sscanf(entry->d_name, WAL_FILE_PATTERN, &n) == 1)
            nums[num_count++] = n;
    }
    closedir(dir);

    qsort(nums, num_count, sizeof(uint64_t), cmp_u64);

    int replayed = 0;
    for (size_t f = 0; f < num_count; f++) {
        char path[2048];
        snprintf(path, sizeof(path), "%s/" WAL_FILE_PATTERN, wal_dir, nums[f]);

        FILE* fp = fopen(path, "rb");
        if (!fp) continue;

        /* read records until EOF or a truncated/corrupt record */
        while (1) {
            uint8_t op;
            if (fread(&op, 1, 1, fp) != 1) break;

            uint16_t cf_len;
            if (fread(&cf_len, sizeof(uint16_t), 1, fp) != 1) break;

            char* cf_name = malloc((size_t)cf_len + 1);
            if (!cf_name) break;
            if (fread(cf_name, 1, cf_len, fp) != cf_len) { free(cf_name); break; }
            cf_name[cf_len] = '\0';

            uint32_t key_size;
            if (fread(&key_size, sizeof(uint32_t), 1, fp) != 1) { free(cf_name); break; }

            uint8_t* key = malloc(key_size);
            if (!key) { free(cf_name); break; }
            if (fread(key, 1, key_size, fp) != key_size) { free(key); free(cf_name); break; }

            uint32_t value_size;
            if (fread(&value_size, sizeof(uint32_t), 1, fp) != 1) {
                free(key); free(cf_name); break;
            }

            uint8_t* value = NULL;
            if (value_size > 0) {
                value = malloc(value_size);
                if (!value) { free(key); free(cf_name); break; }
                if (fread(value, 1, value_size, fp) != value_size) {
                    free(value); free(key); free(cf_name); break;
                }
            }

            wal_record_t rec = {
                .op_type    = op,
                .cf_name    = cf_name,
                .key        = key,
                .key_size   = key_size,
                .value      = value,
                .value_size = value_size,
            };

            int rc = fn(&rec, ctx);
            replayed++;

            free(cf_name);
            free(key);
            free(value);

            if (rc != 0) goto done;
        }
        fclose(fp);
        fp = NULL;
        continue;
done:
        if (fp) fclose(fp);
        break;
    }

    return replayed;
}

void wal_clear(const char* db_path) {
    char wal_dir[1024];
    snprintf(wal_dir, sizeof(wal_dir), "%s/wal", db_path);

    DIR* dir = opendir(wal_dir);
    if (!dir) return;

    struct dirent* entry;
    while ((entry = readdir(dir)) != NULL) {
        uint64_t n;
        if (sscanf(entry->d_name, WAL_FILE_PATTERN, &n) == 1) {
            char path[2048];
            snprintf(path, sizeof(path), "%s/" WAL_FILE_PATTERN, wal_dir, n);
            remove(path);
        }
    }
    closedir(dir);
}
