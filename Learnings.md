Since it's my first time writing an actual real world project in C, i'm noting down the things i've forgotten. I've used C during my first sem in college but left it after that.

- If you write #include"assert.h" it first searches in user local dir and if it doesn't find then it searches in system dir, it's better to use <> for standard library and "" for user defined code.
- Use _t for types it indicates that it's a type.
- Use typedef struct {} typ, cause it'll automatically do the aliasing for you, otherwise you have to write struct log * log in function arguments.
- use __ naming convention for tests. Eg: if you are writing tests for log.c then keep it's name as log__tests.c
- to compile with gcc you have to link all the relevant code files
- #defines are preprocessor declaratives, i.e compiler replaces every line with what you have defined it during the compilation phase
- use make file to build your code
- if you haven't implemented a func in .c file even then the code compiles (interesting)
- In this project Alex paduro uses this style of returning int for each operation Eg: like in log_create etc.. the idea is to have C error handling patterns if it returns 0 it's successful otherwise there's an issue
- Pointers recap
  - & gets the address of a variable
  - Single pointers (*) access data
  - Double pointers (**) change where pointers point
  - For allocation functions, you must use double pointers to modify the caller's pointer
- If a function takes zero arguments then you need to put void in it, this is a modern restriction because empty arguments can also mean any number of arguments in old C (K&R style)
- We can use #pragma once instead of the #ifndef #def #endif declaratives
- First start with the header file, it should have the public api's defined(like an interface) then you can start filling in the implementation it's easy to follow with this approach.
- if mkdir() fails then os sets errno variable it's a thread local variable, there are various encodings like dir already exists, no_space, permission_issue,
we need to reset errno since stale entries are not reset by os
- snprintf() does string concatenation it also checks for buffer overflows
- In Cmake if you make changes in something you don't need to rebuild the whole project you can just run cmake --build build
-
