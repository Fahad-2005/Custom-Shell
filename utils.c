#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"

/* ----------------- Built-ins ----------------- */
int is_builtin(const char *name) {
    if (!name) return 0;
    return strcmp(name,"cd")==0 || strcmp(name,"pwd")==0 || strcmp(name,"help")==0 || strcmp(name,"exit")==0;
}

int run_builtin(struct command *cmd) {
    if (!cmd || cmd->argc==0) return -1;

    if (strcmp(cmd->argv[0],"cd")==0) {
        if (cmd->argc<2) {
            char *home = getenv("HOME");
            if (home) chdir(home);
        } else {
            if (chdir(cmd->argv[1])!=0) perror("cd");
        }
        return 0;
    }

    if (strcmp(cmd->argv[0],"pwd")==0) {
        char cwd[512];
        if (getcwd(cwd,sizeof(cwd))) printf("%s\n",cwd);
        return 0;
    }

    if (strcmp(cmd->argv[0],"help")==0) {
        printf("Built-in commands: cd, pwd, help, exit\n");
        return 0;
    }

    return -1;
}

/* ----------------- Job & Signal Stubs ----------------- */
void init_jobs() { /* Stage 5: initialize job table */ }
void cleanup_jobs() { /* Stage 5: cleanup jobs */ }

void init_signals() { /* Stage 6: setup SIGINT, SIGTSTP, SIGCHLD */ }
void cleanup_signals() { /* Stage 6: cleanup signal handlers */ }

/* ----------------- History Stubs ----------------- */
void init_history() { /* Stage 6: init history */ }
void add_history(const char *cmd) { /* Stage 6: add command */ }
void print_history() { /* Stage 6: print history */ }

/* ----------------- Shell init/cleanup ----------------- */
void init_shell() {
    init_jobs();
    init_signals();
    init_history();
}

void cleanup_shell() {
    cleanup_jobs();
    cleanup_signals();
}
