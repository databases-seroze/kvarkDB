cmake_minimum_required(VERSION 3.10)
project(kvarkDB LANGUAGES C)

# ===== Compiler Options =====
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -pedantic")

# ===== Dependency Paths =====
# Set all paths first before using them
set(LZ4_ROOT /opt/homebrew/opt/lz4)
set(ZSTD_ROOT /opt/homebrew/opt/zstd)
set(SNAPPY_ROOT /opt/homebrew/opt/snappy)

# ===== Include/Link Directories =====
include_directories(
    ${LZ4_ROOT}/include
    ${ZSTD_ROOT}/include
    ${SNAPPY_ROOT}/include
)

link_directories(
    ${LZ4_ROOT}/lib
    ${ZSTD_ROOT}/lib
    ${SNAPPY_ROOT}/lib
)

# ===== Main Library =====
add_library(kvarkdb STATIC
    src/wal.c
    src/bloomfilter.c
    src/compress.c  # Make sure this is included if you're using compression
    src/fnv_hash.c
)

target_link_libraries(kvarkdb PUBLIC
    lz4
    zstd
    snappy
    pthread
)

# ===== Logging Test =====
add_executable(wal_tests
    test/wal__tests.c
)
add_executable(compress_tests
    test/compress__tests.c
)
add_executable(bloomfilter_tests
    test/bloomfilter__tests.c
)

target_link_libraries(wal_tests PRIVATE kvarkdb)
target_link_libraries(compress_tests PRIVATE kvarkdb)
target_link_libraries(bloomfilter_tests PRIVATE kvarkdb)

# ===== Testing Support =====
enable_testing()
add_test(NAME WalTests COMMAND wal_tests)

# ===== Installation (Optional) =====
install(TARGETS kvarkdb DESTINATION lib)
install(FILES src/wal.h DESTINATION include)
