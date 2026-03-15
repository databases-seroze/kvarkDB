#pragma once

#include "kvarkdb.h"

/*
 * Start an interactive REPL session on the given open database.
 * Reads from stdin, writes to stdout/stderr.
 * Returns when the user types "exit" or "quit", or on EOF.
 */
void repl_run(kvarkdb_t* db);
