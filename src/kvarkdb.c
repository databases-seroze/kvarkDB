#include "kvarkdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <errno.h>
#include <limits.h>

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
    char data_path[PATH_MAX];
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
    char meta_path[PATH_MAX];
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

int kvarkdb_open(kvarkdb_t** db, const kvarkdb_config_t* options) {
    *db = malloc(sizeof(kvarkdb_t));
    if (!db) return -1;

    // copy options
    (*db)->config.memtable_max_size = options->memtable_max_size;
    (*db)->config.sstable_target_size = options->sstable_target_size;
    (*db)->config.db_path = options->db_path;

    if (!(*db)->config.db_path) {
        free(db);
        return -1;
    }

    // Initialize empty column families list
    (*db)->column_families = NULL;
    (*db)->cf_count = 0;

    // Create directory structure
    if (create_db_directories(path) != 0) {
        free(db->config.db_path); free(db); return NULL; }

    // Load column families metadata
    // at present you can only creat paths of max size 1023 (last char is \0)
    char cf_path[PATH_MAX];
    if (snprintf(cf_path, sizeof(cf_path), "%s/%s/%s", path, DATA_DIR, COLUMN_FAMILIES_FILE) >= sizeof(cf_path)) {
        fprintf(stderr, "Column families path too long\n");
        free(db->config.db_path);
        free(db);
        return NULL;
    }

    FILE* cf_file = fopen(cf_path, "r");
    if (cf_file) {
        char line[256];
        while (fgets(line, sizeof(line), cf_file)) {
            // Trim newline and check for empty lines
            line[strcspn(line, "\n")] = '\0';
            if (line[0] == '\0') continue;

            if (kvarkdb_create_column_family(db, line) != 0) {
                fclose(cf_file);
                kvarkdb_close(db);
                return NULL;
            }
        }
        fclose(cf_file);
    } else if (errno != ENOENT) {
        perror("Failed to open column families file");
        kvarkdb_close(db);
        return NULL;
    }

    return db;
}
