#include "../src/wal.h"
#include "test_macros.h"
#include <assert.h>
#include <stdio.h>
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#define TEST_DB_DIR "../db"

static int setup(void) {
    // Create test directory
    mkdir(TEST_DB_DIR, 0755);
    // mkdir(TEST_DB_DIR "/wal", 0755);
    return 0;
}

void clear_directory(const char* dir_path) {

    printf("Deleting %s\n", dir_path);
    struct dirent* entry;
    DIR* dir = opendir(dir_path);

    if (dir == NULL) {
        perror("Failed to open directory");
        return;
    }

    while ((entry = readdir(dir)) != NULL) {
        // Skip "." and ".."
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        // Build full file path
        char file_path[1024];
        snprintf(file_path, sizeof(file_path), "%s/%s", dir_path, entry->d_name);

        struct stat st;
        if (stat(file_path, &st) == 0) {
            // If it's a directory, recursively clear it
            if (S_ISDIR(st.st_mode)) {
                clear_directory(file_path); // Recursively clear subdirectories
                rmdir(file_path);  // Remove the empty directory
            } else {
                // It's a file, remove it
                if (remove(file_path) != 0) {
                    perror("Failed to remove file");
                }
            }
        }
    }

    closedir(dir);
}

void tear_down(void) {
    clear_directory(TEST_DB_DIR);
}

void test_wal_init(void) {
    setup();
    WAL* wal = NULL;
    wal = wal_init(TEST_DB_DIR, 1024*1024); // 1MB max size
    assert(wal!=NULL);
    clear_directory(TEST_DB_DIR);
    printf(GREEN "test_wal_init passed\n");
}

void test_wal_rotation(void) {
    WAL* wal = wal_init(TEST_DB_DIR, 10);  // Small size to force rotation

    // Write enough data to trigger rotation
    assert(wal_write(wal, "123456789", 9));  // 9 bytes
    assert(wal_write(wal, "123456789", 9));  // 18 bytes > 10, should rotate

    // Verify two files exist
    char* path1 = malloc(strlen(TEST_DB_DIR) + 20);
    sprintf(path1, "%s/wal/wal_000001.log", TEST_DB_DIR);

    char* path2 = malloc(strlen(TEST_DB_DIR) + 20);
    sprintf(path2, "%s/wal/wal_000002.log", TEST_DB_DIR);

    assert(access(path1, F_OK) == 0);
    assert(access(path2, F_OK) == 0);

    free(path1);
    free(path2);
    wal_close(wal);
    printf(GREEN "test_wal_rotation passed\n");
}

int main(void) {
    printf(GREEN "Running wal tests....\n");
    test_wal_init();
    test_wal_rotation();
}
