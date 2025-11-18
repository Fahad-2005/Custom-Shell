#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"

/* ------------ BUILTINS ------------- */

int is_builtin(const char *name) {
    if (!name) return 0;
    return (strcmp(name, "cd") == 0) ||
           (strcmp(name, "pwd") == 0) ||
           (strcmp(name, "help") == 0) ||
           (strcmp(name, "history") == 0) ||
           (strcmp(name, "jobs") == 0) ||
           (strcmp(name, "fg") == 0) ||
           (strcmp(name, "bg") == 0) ||
           (strcmp(name, "exit") == 0);
}

int run_builtin(struct command *cmd) {
    if (!cmd || cmd->argc == 0) return -1;

    if (strcmp(cmd->argv[0], "cd") == 0) {
        if (cmd->argc < 2) {
            char *home = getenv("HOME");
            if (home) chdir(home);
        } else {
            if (chdir(cmd->argv[1]) != 0) perror("cd");
        }
        return 0;
    }

    if (strcmp(cmd->argv[0], "pwd") == 0) {
        char buf[512];
        if (getcwd(buf, sizeof(buf))) printf("%s\n", buf);
        return 0;
    }

    if (strcmp(cmd->argv[0], "help") == 0) {
        printf("Built-ins: cd pwd help exit history jobs fg bg\n");
        return 0;
    }

    if (strcmp(cmd->argv[0], "history") == 0) {
        print_history();
        return 0;
    }

    if (strcmp(cmd->argv[0], "jobs") == 0) {
        printf("jobs: not implemented yet\n");
        return 0;
    }

    if (strcmp(cmd->argv[0], "fg") == 0) {
        printf("fg: not implemented yet\n");
        return 0;
    }

    if (strcmp(cmd->argv[0], "bg") == 0) {
        printf("bg: not implemented yet\n");
        return 0;
    }

    if (strcmp(cmd->argv[0], "exit") == 0) {
        exit(0);
    }

    return -1;
}

/* ------------ HISTORY (simple in-memory stub) ------------- */
#define HISTORY_MAX 500
static char *history[HISTORY_MAX];
static int history_count = 0;

void init_history() {
    history_count = 0;
}

void add_history(const char *cmd) {
    if (!cmd) return;
    if (history_count == HISTORY_MAX) {
        free(history[0]);
        memmove(history, history+1, sizeof(char*)*(HISTORY_MAX-1));
        history_count--;
    }
    history[history_count++] = strdup(cmd);
}

void print_history() {
    for (int i = 0; i < history_count; ++i) {
        printf("%4d  %s", i+1, history[i]);
        if (history[i][strlen(history[i])-1] != '\n') printf("\n");
    }
}

/* ------------ JOBS (stubs) ------------- */
void init_jobs() { }
void cleanup_jobs() { }

/* ------------ SIGNALS (stubs) ------------- */
void init_signals() { }
void cleanup_signals() { }

/* ------------ INIT / CLEANUP ------------- */
void init_shell() {
    init_history();
    init_jobs();
    init_signals();
}

void cleanup_shell() {
    cleanup_jobs();
    cleanup_signals();
    /* free history */
    for (int i=0;i<history_count;i++) free(history[i]);
}
