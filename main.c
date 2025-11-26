//========main.c=========//
#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <readline/readline.h>
#include <readline/history.h>
#include "shell.h"

// ------------------- AUTOCOMPLETE COMMAND LIST -------------------
static const char *cmd_list[] = {
    "cd",
    "pwd",
    "here",
    "tasks",
    "ls",
    "look",
    "say",
    "edit",
    "bring",
    "runbg",
    "sleep",
    NULL
};

// Autocomplete generator (returns one match at a time)
char *cmd_generator(const char *text, int state) {
    static int index;
    const char *name;

    if (state == 0) {
        index = 0;  // first time reset
    }

    while ((name = cmd_list[index++])) {
        if (strncmp(name, text, strlen(text)) == 0) {
            return strdup(name);
        }
    }
    return NULL;
}

// Tell readline how to autocomplete words
char **cmd_completion(const char *text, int start, int end) {
    (void)end;

    // Only autocomplete the first word
    if (start == 0)
        return rl_completion_matches(text, cmd_generator);

    // For second+ words → no autocomplete
    return NULL;
}

// ------------------- SIGNAL HANDLING -------------------
void sigint_handler(int sig) {
    (void)sig;
    printf("\n");
    rl_on_new_line();
    rl_replace_line("", 0);
    rl_redisplay();
}

int main(void) {
    // Handle Ctrl+C
    signal(SIGINT, sigint_handler);

    // Initialize shell
    init_shell();

    // Initialize readline history (no file loaded)
    using_history();
    stifle_history(500);

    rl_instream = stdin;
    rl_outstream = stdout;

    // Set autocomplete handler
    rl_attempted_completion_function = cmd_completion;

    char *line;
    while (1) {
        line = readline("customshell> ");

        if (!line) {
            printf("\n");
            break; // user pressed Ctrl+D
        }

        // Trim leading spaces
        char *trimmed = line;
        while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

        // Add to session history only if non-empty
        if (*trimmed)
            add_history(line);

        struct command *cmd = parse_line(line);

        if (cmd) {
            // Built-in quit
            if (cmd->argc == 1 && strcmp(cmd->argv[0], "quit") == 0) {
                free(line);
                free_command(cmd);
                break;
            }

            execute_command(cmd);
            free_command(cmd);
        }

        free(line);
    }

    cleanup_shell();
    return 0;
}

