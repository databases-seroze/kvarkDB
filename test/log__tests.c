// #include "log.h"
#include "../src/log.h"
#include <assert.h>

void test_log_init(void){
    log_t* log = NULL;
    log_create(&log, "database.log", true);
    assert(log!=NULL);
    printf("test log init worked\n");
}

int main(void){
    printf("Running log tests...\n");
    test_log_init();
}
