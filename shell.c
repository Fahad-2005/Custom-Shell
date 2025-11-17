#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include "shell.h"
#include "commands.h"
#include "utils.h"

#define MAX_ARGS 64

// Stage 2 & 3: Parse and execute command
void execute_command(char *input) {
    char *args[MAX_ARGS];
    int argc = 0;

    // Stage 2: Parsing
    char *token = strtok(input, " "); // split by space
    while (token != NULL && argc < MAX_ARGS - 1) {
        args[argc++] = token;
        token = strtok(NULL, " ");
    }
    args[argc] = NULL; // Null-terminate args array

    if (argc == 0) return; // empty command

    // Stage 3: Execute built-in commands
    if (strcmp(args[0], "cd") == 0) {
        builtin_cd(args);
        return;
    }
    if (strcmp(args[0], "help") == 0) {
        builtin_help();
        return;
    }
    if (strcmp(args[0], "exit") == 0) {
        // Handled in main.c
        return;
    }

    // Stage 3: Execute external commands
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork failed");
        return;
    }

    if (pid == 0) {
        // Child process
        if (execvp(args[0], args) < 0) {
            perror("Command execution failed");
            exit(EXIT_FAILURE);
        }
    } else {
        // Parent process waits for child
        wait(NULL);
    }
}
