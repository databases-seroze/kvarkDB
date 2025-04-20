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
