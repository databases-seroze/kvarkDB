// #include "log.h"
#include "../src/log.h"
#include <assert.h>

void test_log_init(){
    log_t* log = log_create("database.log", true);
    assert(log!=NULL);
    printf("test log init worked\n");
}

int main(void){
    printf("Running log tests...\n");
    test_log_init();
}
