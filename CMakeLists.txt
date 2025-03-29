cmake_minimum_required(VERSION 3.10)
project(kvarkDB LANGUAGES C)

# ===== Compiler Options =====
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_FLAGS "-Wall -Wextra -Werror")

# ===== Main Library =====
add_library(kvarkdb STATIC
    src/log.c
)

# ===== Logging Test =====
add_executable(log_tests
    test/log__tests.c
)
target_link_libraries(log_tests PRIVATE kvarkdb)

# ===== Installation (Optional) =====
install(TARGETS kvarkdb DESTINATION lib)
install(FILES src/log.h DESTINATION include)
