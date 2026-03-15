#include "kvarkdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>

/* maximum supported path length — avoids relying on KVARKDB_MAX_PATH which
 * is optional on POSIX and varies across platforms (Linux 4096, macOS 1024) */
#define KVARKDB_MAX_PATH 1024

#define META_DIR "meta"
#define DATA_DIR "data"
#define COLUMN_FAMILIES_FILE "meta/column_families"

static int create_db_directories(const char* path) {
    // Create root directory (only if doesn't exist)
    if (mkdir(path, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create root directory");
        return -1;
    }
    //reset error no
    errno = 0;

    // Create data directory
    char data_path[KVARKDB_MAX_PATH];
    if (snprintf(data_path, sizeof(data_path), "%s/%s", path, DATA_DIR) >= sizeof(data_path)) {
        fprintf(stderr, "Data directory path too long\n");
        return -1;
    }
    if (mkdir(data_path, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create data directory");
        return -1;
    }
    //reset error no
    errno = 0;

    // Create meta directory
    char meta_path[KVARKDB_MAX_PATH];
    if (snprintf(meta_path, sizeof(meta_path), "%s/%s/%s", path, DATA_DIR, META_DIR) >= sizeof(meta_path)) {
        fprintf(stderr, "Meta directory path too long\n");
        return -1;
    }
    if (mkdir(meta_path, 0755) != 0 && errno != EEXIST) {
        perror("Failed to create meta directory");
        return -1;
    }
    //reset error no
    errno = 0;

    return 0;
}

int kvarkdb_open(kvarkdb_t* db) {
    if (!db) return -1;

    if (!db->config.db_path) return -1;

    // Initialize empty column families list
    db->column_families = NULL;
    db->column_family_count = 0;

    // Create directory structure
    if (create_db_directories(db->config.db_path) != 0) {
        return -1;
    }

    // Load column families metadata
    // paths are limited to KVARKDB_MAX_PATH - 1 characters (last char is \0)
    char cf_path[KVARKDB_MAX_PATH];
    if (snprintf(cf_path, sizeof(cf_path), "%s/%s/%s", db->config.db_path, DATA_DIR, COLUMN_FAMILIES_FILE) >= (int)sizeof(cf_path)) {
        fprintf(stderr, "Column families path too long\n");
        return -1;
    }

    FILE* cf_file = fopen(cf_path, "r");
    if (cf_file) {
        char line[256];
        while (fgets(line, sizeof(line), cf_file)) {
            // Trim newline and check for empty lines
            line[strcspn(line, "\n")] = '\0';
            if (line[0] == '\0') continue;

            if (kvarddb_create_column_family(db, line) != 0) {
                fclose(cf_file);
                kvarkdb_close(db);
                return -1;
            }
        }
        fclose(cf_file);
    } else if (errno != ENOENT) {
        perror("Failed to open column families file");
        kvarkdb_close(db);
        return -1;
    }

    return 0;
}
