#include "repl.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define REPL_MAX_LINE 4096

/* ------------------------------------------------------------------ */
/* helpers                                                              */

static void print_help(void) {
    printf(
        "Commands:\n"
        "  use <cf>           Set active column family (creates it if it does not exist)\n"
        "  put <key> <value>  Write a key-value pair to the active column family\n"
        "  get <key>          Read a key from the active column family\n"
        "  delete <key>       Delete a key from the active column family\n"
        "  list cfs           List all column families\n"
        "  drop cf <name>     Drop a column family\n"
        "  help               Show this help\n"
        "  exit | quit        Close the database and exit\n"
    );
}

/* trim trailing newline / carriage return in-place */
static void trim_newline(char* s) {
    size_t n = strlen(s);
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = '\0';
}

/* split line into at most max_parts whitespace-delimited tokens.
 * modifies line in-place; fills parts[] with pointers into it.
 * returns the number of parts found. */
static int split(char* line, char** parts, int max_parts) {
    int n = 0;
    char* p = line;
    while (*p && n < max_parts) {
        /* skip leading spaces */
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') break;
        parts[n++] = p;
        /* find end of token */
        while (*p && *p != ' ' && *p != '\t') p++;
        if (*p) *p++ = '\0';
    }
    return n;
}

/* ------------------------------------------------------------------ */
/* command handlers                                                     */

static void cmd_use(kvarkdb_t* db, char* active_cf, const char* name) {
    /* create CF if it doesn't exist yet; ignore error if it already exists */
    kvarddb_create_column_family(db, name);

    /* verify it now exists */
    int found = 0;
    for (size_t i = 0; i < db->column_family_count; i++) {
        if (strcmp(db->column_families[i].name, name) == 0) { found = 1; break; }
    }
    if (!found) {
        fprintf(stderr, "error: could not create or find column family '%s'\n", name);
        return;
    }
    strncpy(active_cf, name, REPL_MAX_LINE - 1);
    active_cf[REPL_MAX_LINE - 1] = '\0';
    printf("using column family '%s'\n", active_cf);
}

static void cmd_put(kvarkdb_t* db, const char* cf, const char* key, const char* value) {
    if (kvarkdb_put(db, cf, key, value) == 0)
        printf("ok\n");
    else
        fprintf(stderr, "error: put failed\n");
}

static void cmd_get(kvarkdb_t* db, const char* cf, const char* key) {
    char* value = kvarkdb_get(db, cf, key);
    if (value) {
        printf("%s\n", value);
        free(value);
    } else {
        printf("(not found)\n");
    }
}

static void cmd_delete(kvarkdb_t* db, const char* cf, const char* key) {
    if (kvarkdb_delete(db, cf, key) == 0)
        printf("ok\n");
    else
        fprintf(stderr, "error: delete failed\n");
}

static void cmd_list_cfs(kvarkdb_t* db) {
    if (db->column_family_count == 0) {
        printf("(no column families)\n");
        return;
    }
    for (size_t i = 0; i < db->column_family_count; i++)
        printf("  %s\n", db->column_families[i].name);
}

static void cmd_drop_cf(kvarkdb_t* db, char* active_cf, const char* name) {
    if (kvarddb_drop_column_family(db, name) == 0) {
        printf("dropped '%s'\n", name);
        /* clear active CF if it was the dropped one */
        if (strcmp(active_cf, name) == 0) active_cf[0] = '\0';
    } else {
        fprintf(stderr, "error: could not drop column family '%s'\n", name);
    }
}

/* ------------------------------------------------------------------ */
/* main loop                                                            */

void repl_run(kvarkdb_t* db) {
    char line[REPL_MAX_LINE];
    char active_cf[REPL_MAX_LINE] = {0};

    printf("kvarkDB REPL — type 'help' for commands\n");

    while (1) {
        if (active_cf[0])
            printf("%s> ", active_cf);
        else
            printf("> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) break;  /* EOF */
        trim_newline(line);
        if (line[0] == '\0') continue;

        char* parts[4];
        int n = split(line, parts, 4);
        if (n == 0) continue;

        /* exit / quit */
        if (strcmp(parts[0], "exit") == 0 || strcmp(parts[0], "quit") == 0) {
            printf("bye\n");
            break;
        }

        /* help */
        if (strcmp(parts[0], "help") == 0) {
            print_help();
            continue;
        }

        /* use <cf> */
        if (strcmp(parts[0], "use") == 0) {
            if (n < 2) { fprintf(stderr, "usage: use <cf>\n"); continue; }
            cmd_use(db, active_cf, parts[1]);
            continue;
        }

        /* list cfs */
        if (strcmp(parts[0], "list") == 0 && n >= 2 && strcmp(parts[1], "cfs") == 0) {
            cmd_list_cfs(db);
            continue;
        }

        /* drop cf <name> */
        if (strcmp(parts[0], "drop") == 0 && n >= 3 && strcmp(parts[1], "cf") == 0) {
            cmd_drop_cf(db, active_cf, parts[2]);
            continue;
        }

        /* commands that require an active CF */
        if (active_cf[0] == '\0') {
            fprintf(stderr, "no active column family — run 'use <cf>' first\n");
            continue;
        }

        /* put <key> <value> */
        if (strcmp(parts[0], "put") == 0) {
            if (n < 3) { fprintf(stderr, "usage: put <key> <value>\n"); continue; }
            cmd_put(db, active_cf, parts[1], parts[2]);
            continue;
        }

        /* get <key> */
        if (strcmp(parts[0], "get") == 0) {
            if (n < 2) { fprintf(stderr, "usage: get <key>\n"); continue; }
            cmd_get(db, active_cf, parts[1]);
            continue;
        }

        /* delete <key> */
        if (strcmp(parts[0], "delete") == 0) {
            if (n < 2) { fprintf(stderr, "usage: delete <key>\n"); continue; }
            cmd_delete(db, active_cf, parts[1]);
            continue;
        }

        fprintf(stderr, "unknown command '%s' — type 'help'\n", parts[0]);
    }
}
