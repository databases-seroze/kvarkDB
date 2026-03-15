cmake_minimum_required(VERSION 3.10)
project(kvarkDB LANGUAGES C)

# ===== Compiler Options =====
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -pedantic")

# ===== Required Dependencies =====
find_package(PkgConfig REQUIRED)

pkg_check_modules(LZ4 REQUIRED liblz4)
pkg_check_modules(ZSTD REQUIRED libzstd)
pkg_check_modules(SNAPPY REQUIRED snappy)

find_package(Threads REQUIRED)

# ===== Main Library =====
add_library(kvarkdb STATIC
    src/wal.c
    src/bloomfilter.c
    src/compress.c
    src/fnv_hash.c
    src/skiplist.c
    src/memtable.c
    src/sstable.c
    src/kvarkdb.c
)

target_include_directories(kvarkdb PUBLIC
    ${LZ4_INCLUDE_DIRS}
    ${ZSTD_INCLUDE_DIRS}
    ${SNAPPY_INCLUDE_DIRS}
)

target_link_libraries(kvarkdb PUBLIC
    ${LZ4_LIBRARIES}
    ${ZSTD_LIBRARIES}
    ${SNAPPY_LIBRARIES}
    Threads::Threads
)

# ===== Tests =====
add_executable(wal_tests test/wal__tests.c)
add_executable(compress_tests test/compress__tests.c)
add_executable(bloomfilter_tests test/bloomfilter__tests.c)
add_executable(skiplist_tests test/skiplist__tests.c)
add_executable(memtable_tests test/memtable__tests.c)
add_executable(sstable_tests test/sstable__tests.c)
add_executable(kvarkdb_tests test/kvardkdb__tests.c)

target_link_libraries(wal_tests PRIVATE kvarkdb)
target_link_libraries(compress_tests PRIVATE kvarkdb)
target_link_libraries(bloomfilter_tests PRIVATE kvarkdb)
target_link_libraries(skiplist_tests PRIVATE kvarkdb)
target_link_libraries(memtable_tests PRIVATE kvarkdb)
target_link_libraries(sstable_tests PRIVATE kvarkdb)
target_link_libraries(kvarkdb_tests PRIVATE kvarkdb)

# ===== Testing Support =====
enable_testing()
add_test(NAME WalTests COMMAND wal_tests)
add_test(NAME CompressTests COMMAND compress_tests)
add_test(NAME BloomfilterTests COMMAND bloomfilter_tests)
add_test(NAME SkiplistTests COMMAND skiplist_tests)
add_test(NAME MemtableTests COMMAND memtable_tests)
add_test(NAME SstableTests COMMAND sstable_tests)
add_test(NAME KvarkdbTests COMMAND kvarkdb_tests)

# ===== Installation =====
install(TARGETS kvarkdb DESTINATION lib)
install(FILES src/wal.h DESTINATION include)