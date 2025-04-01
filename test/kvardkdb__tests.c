#include "../src/kvarkdb.h"

void test_kvarkdb_open_close(){
    kvarkdb_t* db = NULL;

    // Initialize configuration with reasonable defaults
    kvarkdb_config_t config = {
        .memtable_max_size = 1024 * 1024,
        .sstable_target_size = 2 * 1024 * 1024,
        .db_path = "test_db", // 1MB memtable threshold
              // Don't error if DB exists
    };

    err = kvarkdb_open(&config);

    if (err != NULL)
    {
        printf(RED "%s" RESET, err->message);
    }
    assert(err == NULL);
}

int main(void){
    test_kvarkdb_open_close();

}
