#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "shell.h"

int main() {
    // ----------------- Launch in a new terminal -----------------
    if (!getenv("MYSHELL_LAUNCHED")) {
        setenv("MYSHELL_LAUNCHED", "1", 1);
        system("gnome-terminal -- bash -c './myshell; exec bash'");
        return 0;
    }

    // ----------------- Initialize shell -----------------
    init_shell();

    char *line;

    while (1) {
        line = readline("customshell> ");
        if (!line) break; // EOF / Ctrl-D

        if (*line) add_history(line); // Add to readline history

        struct command *cmd = parse_line(line);
        free(line); // readline malloc'd it
        if (!cmd) continue;

        // Built-in quit
        if (cmd->argc == 1 && strcmp(cmd->argv[0], "quit") == 0) {
            free_command(cmd);
            break;
        }

        execute_command(cmd);
        free_command(cmd);
    }

    cleanup_shell();
    return 0;
}
