CMake is a build system generator — it doesn't compile your code directly, it generates the build files (like Makefile) that then compile your code.

CMakeLists.txt  →  cmake  →  Makefile  →  make  →  binary
   (you write)              (generated)           (your program)

The three commands you'll use every day

# 1. generate build files (run once, or after editing CMakeLists.txt)
cmake -B build -DCMAKE_BUILD_TYPE=Debug

# 2. compile
cmake --build build

# 3. run
./build/neapdb_test
```

---

**Project layout it expects**
```
neapdb/
├── CMakeLists.txt
├── include/
│   └── neapdb.h       # public headers
├── src/
│   ├── db.c
│   ├── platform.c
│   └── error.c
└── tests/
    └── test_main.c

-S means source directory (where CMakeLists.txt is), -B means build directory. When you omit -S it just uses wherever you currently are. That's why running from the root folder is the convention.

cmake -S /path/to/project -B /path/to/project/build