cmake_minimum_required(VERSION 3.10)
project(kvarkDB LANGUAGES C)

# ===== Compiler Options =====
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror -pedantic")

# ===== Main Library =====
add_library(kvarkdb STATIC
    src/wal.c
)

# ===== Logging Test =====
add_executable(wal_tests
    test/wal__tests.c
)
target_link_libraries(wal_tests PRIVATE kvarkdb)

# ===== Testing Support =====
enable_testing()

add_test(NAME WalTests COMMAND wal_tests)

# ===== Installation (Optional) =====
install(TARGETS kvarkdb DESTINATION lib)
install(FILES src/wal.h DESTINATION include)
