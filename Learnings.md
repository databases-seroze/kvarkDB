Since it's my first time writing an actual real world project in C, i'm noting down the things i've forgotten.
I've used C during my first sem in college but left it after that.

- If you write #include"assert.h" it first searches in user local dir and if it doesn't find then it searches in system dir, it's better to use <> for standard library and "" for user defined code.
- Use _t for types it indicates that it's a type.
- Use typedef struct {} typ, cause it'll automatically do the aliasing for you, otherwise you have to write struct log * log in function arguments.
- use __ naming convention for tests. Eg: if you are writing tests for log.c then keep it's name as log__tests.c
- to compile with gcc you have to link all the relevant code files
- #defines are preprocessor declaratives, i.e compiler replaces every line with what you have defined it during the compilation phase
- use make file to build your code
- if you haven't implemented a func in .c file even then the code compiles (interesting)
-
