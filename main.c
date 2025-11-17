#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"

#define INPUT_MAX 2048

int main() {
    char line[INPUT_MAX];

    init_shell();  // initialize jobs, signals, history

    while (1) {
        char cwd[512];
        if (getcwd(cwd, sizeof(cwd)) != NULL)
            printf("customshell:%s> ", cwd);
        else
            printf("customshell> ");
        fflush(stdout);

        if (!fgets(line, sizeof(line), stdin)) {
            printf("\n");
            break;
        }
        line[strcspn(line, "\n")] = '\0';
        if (line[0] == '\0') continue;

        struct command *cmd = parse_line(line);
        if (!cmd) continue;

        if (cmd->argc == 1 && strcmp(cmd->argv[0], "exit") == 0) {
            free_command(cmd);
            break;
        }

        execute_command(cmd);
        free_command(cmd);
    }

    cleanup_shell();
    return 0;
}
