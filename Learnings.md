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