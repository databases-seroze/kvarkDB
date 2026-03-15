# C Dev Notes

> First time writing a real-world project in C. Noting down things I've forgotten
> since my first semester in college.

---

## General Notes

- `#include ""` searches user local dir first, then system dir. Use `<>` for
  standard libraries and `""` for your own code.

- Use `_t` suffix for types — it signals to the reader that it's a type eg: `wal_entry_t`.

- Use `typedef struct {} type_t` — it automatically aliases the struct so you don't
  have to write `struct log *log` in every function argument.

- Use `__` naming convention for test files. eg: tests for `wal.c` → `wal__tests.c`.

- To compile with gcc you have to manually link all relevant `.c` files. Use CMake
  to avoid doing this by hand.

- `#define` are preprocessor directives — the compiler replaces every occurrence
  with the defined value before compilation starts.

- If you haven't implemented a function declared in a `.h` file the code still
  compiles — it only fails at link time. Interesting quirk.

- In CMake if you make changes you don't need to rebuild the whole project,
  just run `cmake --build build`.

---

## Error Handling Pattern

Return `int` from every operation — `0` means success, anything else is an error.
This is the standard C error handling convention.
```c
int wal_write(wal_t *wal, const char *data, int len);  // 0 = ok, -1 = error
```

---

## Pointers Recap

| Syntax | Meaning |
|--------|---------|
| `&x` | get the address of variable `x` |
| `*p` | access the data at pointer `p` |
| `**p` | change where a pointer points |

> For allocation functions you must use double pointers (`**`) to modify
> the caller's pointer — a single pointer only modifies a local copy.

---

## Misc C Rules

- If a function takes no arguments write `void` explicitly — empty parentheses `()`
  means "any number of arguments" in old K&R style C.

- Use `#pragma once` instead of the `#ifndef / #define / #endif` include guard
  boilerplate — cleaner and does the same thing.

- Start with the header file first — define the public API like an interface,
  then fill in the implementation. Much easier to reason about.

- `snprintf()` does safe string formatting and concatenation — it checks for
  buffer overflows unlike `sprintf()`.

---

## errno

When a syscall like `mkdir()` fails, the OS sets the `errno` variable.
It's thread-local. Common codes:

- Directory already exists
- No space left
- Permission issue

**Always reset `errno` before checking it** — the OS does not clear stale values.

---

## Header Files in C

In C we split code into two files — a `.h` header file and a `.c` source file.

- `.h` defines the **contract** — function signatures, types, constants. Think of it as a menu.
- `.c` has the **actual implementation**. Think of it as the kitchen.

Any file that wants to use `wal_write()` just includes the header —
no need to copy the implementation everywhere.
```c
#include "wal.h"   // just the contract
wal_write(...);    // compiler trusts the signature
```

### Why does C need this?

C's compiler is **single-pass** — it reads each file once, top to bottom.
When it sees a function call it must have already seen that function's declaration,
otherwise it doesn't know if you're calling it correctly.

When the compiler sees `#include "wal.h"` it literally **copy-pastes** the header
contents inline before the pass starts — so all signatures are already there:
```c
// what you write
#include "wal.h"
#include "bloomfilter.h"

int main() { wal_write(...); }

// what the compiler actually sees
int wal_write(const char *path, const char *data, int len);  // pasted from wal.h
void wal_close(int fd);                                      // pasted from wal.h
int bloom_check(const char *key);                            // pasted from bloomfilter.h

int main() { wal_write(...); }  // signature already seen above — ok
```

This design was intentional — in 1972 computers had kilobytes of RAM.
A single-pass compiler uses almost no memory. Loading all files at once
to resolve names was too expensive.

### Why modern languages don't need headers

| Language | Approach |
|----------|----------|
| Python | Interpreter reads the whole file before running |
| Rust | Compiler builds a full dependency graph upfront |
| Go | Compiler scans all files in a package together |
| Java | Multi-pass compiler resolves names in later passes |

The trade-off C made — developer inconvenience in exchange for minimal resource
usage — is actually why C is still used for systems programming today.

is size_t same as uint32 ?
size_t — platform dependent

32-bit system → 4 bytes (same as uint32_t)
64-bit system → 8 bytes (same as uint64_t)
Defined in <stddef.h>
Always unsigned
Specifically meant to represent sizes and counts — array lengths, sizeof() results, malloc() arguments
Use size_t only for in-memory things like loop counters and buffer sizes.

```
// WRONG for on-disk format
size_t key_len;   // 8 bytes on 64-bit, 4 bytes on 32-bit

// CORRECT for on-disk format
uint32_t key_len;  // always 4 bytes everywhere
```

why do we do this #ifndef #def ..#endif12:45 PMYou already noted #pragma once is the modern alternative — this is what the old way looks like and why it exists.
The problem it solves — double inclusion
Imagine this:
c// db.h
#include "wal.h"
#include "bloomfilter.h"
c// main.c
#include "wal.h"   // included once here
#include "db.h"    // db.h includes wal.h again — now wal.h is pasted TWICE
Since #include is just copy-paste, wal.h gets pasted into main.c twice. This causes the compiler to see duplicate type definitions and it errors out.

# C Standard Library Cheatsheet

## Headers Quick Reference

| Function | Header |
|----------|--------|
| `memcmp`, `memcpy`, `memset`, `strlen`, `strcat` | `<string.h>` |
| `malloc`, `free`, `calloc`, `realloc` | `<stdlib.h>` |
| `printf`, `fprintf`, `snprintf` | `<stdio.h>` |
| `open`, `read`, `write`, `close` | `<fcntl.h>` / `<unistd.h>` |
| `pread`, `pwrite`, `fsync` | `<unistd.h>` |
| `pthread_*` | `<pthread.h>` |
| `errno` | `<errno.h>` |
| `uint8_t`, `uint32_t`, `uint64_t`, `int64_t` | `<stdint.h>` |
| `size_t` | `<stddef.h>` |
| `bool`, `true`, `false` | `<stdbool.h>` |
| `INT_MAX`, `INT_MIN`, `SIZE_MAX` | `<limits.h>` |
| `time`, `clock` | `<time.h>` |
| `assert` | `<assert.h>` |

---

## Memory Functions `<string.h>`
```c
memcmp(a, b, n)        // compare n bytes — returns 0 if equal, <0 or >0 if not
memcpy(dst, src, n)    // copy n bytes from src to dst — regions must not overlap
memmove(dst, src, n)   // copy n bytes — safe when regions overlap
memset(ptr, val, n)    // fill n bytes with val (commonly 0 to zero out memory)
strlen(str)            // length of null-terminated string, not counting \0
```

> `memcmp` uses SIMD internally — never write your own comparison loop for raw bytes.
> Always wrap it for variable-length keys:
> ```c
> int key_compare(const uint8_t *a, size_t a_len,
>                 const uint8_t *b, size_t b_len) {
>     size_t min_len = a_len < b_len ? a_len : b_len;
>     int cmp = memcmp(a, b, min_len);
>     if (cmp != 0) return cmp;
>     if (a_len < b_len) return -1;
>     if (a_len > b_len) return  1;
>     return 0;
> }
> ```

---

## Memory Allocation `<stdlib.h>`
```c
malloc(size)           // allocate size bytes — contents are uninitialized
calloc(count, size)    // allocate count * size bytes — zeroed out
realloc(ptr, new_size) // resize an existing allocation
free(ptr)              // release memory — always free what you malloc
```

> Always check the return value — malloc returns NULL if allocation fails:
> ```c
> node_t *node = malloc(sizeof(node_t));
> if (node == NULL) return -1;  // handle failure
> ```

---

## File I/O `<fcntl.h>` `<unistd.h>`
```c
open(path, flags)           // open file — returns fd (file descriptor)
close(fd)                   // close file descriptor
read(fd, buf, n)            // read n bytes — moves file offset forward
write(fd, buf, n)           // write n bytes — moves file offset forward
pread(fd, buf, n, offset)   // read n bytes at offset — does NOT move offset
pwrite(fd, buf, n, offset)  // write n bytes at offset — does NOT move offset
fsync(fd)                   // flush to disk — use after critical writes
```

> Use `pread`/`pwrite` for your WAL and SSTable — they are concurrent-safe
> because they don't move the file offset (no seek + read race condition).

Common `open` flags:
```c
O_RDONLY    // read only
O_WRONLY    // write only
O_RDWR      // read and write
O_CREAT     // create if doesn't exist (needs mode argument)
O_APPEND    // always write to end
O_TRUNC     // truncate file to zero on open
```

---

## Integer Types `<stdint.h>`

| Type | Size | Range |
|------|------|-------|
| `uint8_t` | 1 byte | 0 to 255 |
| `uint16_t` | 2 bytes | 0 to 65,535 |
| `uint32_t` | 4 bytes | 0 to 4,294,967,295 |
| `uint64_t` | 8 bytes | 0 to 18,446,744,073,709,551,615 |
| `int32_t` | 4 bytes | -2,147,483,648 to 2,147,483,647 |
| `int64_t` | 8 bytes | -9,223,372,036,854,775,808 to 9,223,372,036,854,775,807 |

> Use fixed-width types for anything written to disk or sent over a network.
> Never use `int` or `size_t` in on-disk formats — their size changes per platform.

---

## Error Handling `<errno.h>`
```c
#include <errno.h>
#include <string.h>   // for strerror

errno = 0;                          // always reset before a syscall
int result = mkdir(path, 0755);
if (result == -1) {
    printf("Error: %s\n", strerror(errno));  // human readable error
}
```

Common errno values:
```c
ENOENT    // no such file or directory
EEXIST    // file already exists
ENOMEM    // out of memory
EACCES    // permission denied
ENOSPC    // no space left on device
EINVAL    // invalid argument
```

---

## How to look up any function
```bash
man memcmp       # shows header, signature, description, return value
man 2 open       # section 2 = syscalls
man 3 malloc     # section 3 = standard library functions
```

Or search **cppreference.com** — the best C/C++ reference online.

---

## Compaction Implementation Learnings

### Use-after-free in inline-free loops

When deduplicating a sorted array, the first version freed `flat[i].key` inside
the loop, then the next iteration compared against `flat[i-1].key` — which was
now a dangling pointer. Fix: two-pass approach — first mark which entries to keep
(reading pointers, freeing nothing), then free or transfer in a second pass.
Reading and mutating in the same loop is a common source of this bug.

### Double-free when a cleanup function frees the array itself

`free_raw_entries(entries, n)` freed each entry's key/value AND called
`free(entries)` on the array itself. Calling `free(out)` afterwards was a
double-free. Be explicit about whether a cleanup function owns the container
or just its elements — and document it.

### `bloom_add` reads past the null terminator intentionally

`bloom_add` generates multiple hash values by calling
`fnv1a_hash(item, strlen(item) + i)` for `i = 0..num_hashes-1`. For `i > 0`
this intentionally reads `i` bytes past the `'\0'` as a length-based salt.
Even a properly null-terminated key overflows for `i >= 1`. Fix: allocate
`key_size + 1 + BLOOM_NUM_HASHES` bytes with `calloc`, so the over-reads land
in zero-padded memory rather than unallocated space.

### `memcpy` on a struct array copies pointers, not the pointed-to data

`memcpy(flat + pos, per_sst[i], count * sizeof(raw_entry_t))` copies the
struct shells — key/value pointers are now shared between `per_sst[i]` and
`flat`. The right cleanup is `free(per_sst[i])` (shell only), NOT
`free_raw_entries(per_sst[i], n)` which would also free the key/value memory
now owned by `flat`.

### `calloc` as a safety net for partial-initialization cleanup

`calloc` zero-initialises all fields, so pointer fields start as `NULL`.
This means a cleanup function can safely iterate the full array even if only
the first `k` entries were initialised — `free(NULL)` is a no-op. No need to
track a separate "how many entries are initialised" counter in error paths.

### `goto` for cleanup in C

When a function acquires multiple resources (file handle, two heap buffers,
etc.), jump to a single `goto fail` / `goto err` cleanup block at the bottom.
The alternative — duplicating cleanup code at each failure point — gets
unwieldy and creates bugs when you add a new resource and forget to free it in
one of the early-exit paths.