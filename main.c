#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "shell.h"    // For parse and execute function prototypes

#define MAX_INPUT 1024

int main() {
    char input[MAX_INPUT];

    // Stage 1: Initialization
    printf("Welcome to CustomShell!\n");

    while (1) {
        // 1. Print prompt
        printf("CustomShell> ");
        fflush(stdout);

        // 2. Read user input
        if (fgets(input, MAX_INPUT, stdin) == NULL) {
            printf("\n");
            break; // EOF (Ctrl+D) exits shell
        }

        // Remove trailing newline
        input[strcspn(input, "\n")] = '\0';

        // 3. Skip empty input
        if (strlen(input) == 0)
            continue;

        // 4. Check for exit command
        if (strcmp(input, "exit") == 0) {
            printf("Bye!\n");
            break;
        }

        // 5. Send input to parser & executor
        execute_command(input);
    }

    return 0;
}
