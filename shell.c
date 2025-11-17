#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include "shell.h"

/* ----------------- Command Struct ----------------- */
struct command *new_command() {
    struct command *c = malloc(sizeof(struct command));
    c->argv = NULL;
    c->argc = 0;
    c->background = 0;
    c->infile = NULL;
    c->outfile = NULL;
    c->append = 0;
    c->next = NULL;
    return c;
}

void push_arg(struct command *c, const char *arg) {
    c->argv = realloc(c->argv, sizeof(char*)*(c->argc+2));
    c->argv[c->argc] = strdup(arg);
    c->argc++;
    c->argv[c->argc] = NULL;
}

/* ----------------- Parser ----------------- */
struct command *parse_line(const char *line) {
    struct command *head = new_command();
    struct command *curr = head;

    int i = 0, len = strlen(line);
    char token[1024];
    int tpos = 0;
    char quote = 0;

    while (i <= len) {
        char c = line[i];
        if ((c == ' ' || c == '\t' || c == '\0') && quote == 0) {
            if (tpos > 0) {
                token[tpos] = '\0';
                push_arg(curr, token);
                tpos = 0;
            }
        } else if ((c == '\'' || c == '"') && quote == 0) {
            quote = c;
        } else if (c == quote) {
            quote = 0;
        } else if (c == '\\') {
            i++;
            if (i < len) token[tpos++] = line[i];
        } else if (c == '|') {
            if (tpos > 0) {
                token[tpos] = '\0';
                push_arg(curr, token);
                tpos = 0;
            }
            curr->next = new_command();
            curr = curr->next;
        } else if (c == '<' || c == '>') {
            if (tpos > 0) {
                token[tpos] = '\0';
                push_arg(curr, token);
                tpos = 0;
            }
            int append = 0;
            if (c == '>' && line[i+1] == '>') { append = 1; i++; }
            i++;
            while (line[i]==' ' || line[i]=='\t') i++;
            int j=0; char fname[512];
            while (line[i] && line[i]!=' ' && line[i]!='\t' && line[i]!='>' && line[i]!='<' && line[i]!='&') {
                fname[j++] = line[i++];
            }
            fname[j] = '\0';
            i--;
            if (c == '<') curr->infile = strdup(fname);
            else { curr->outfile = strdup(fname); curr->append = append; }
        } else if (c == '&' && quote==0) {
            curr->background = 1;
        } else {
            token[tpos++] = c;
        }
        i++;
    }

    return head;
}

void free_command(struct command *cmd) {
    while (cmd) {
        for (int i=0;i<cmd->argc;i++) free(cmd->argv[i]);
        free(cmd->argv);
        if (cmd->infile) free(cmd->infile);
        if (cmd->outfile) free(cmd->outfile);
        struct command *next = cmd->next;
        free(cmd);
        cmd = next;
    }
}

/* ----------------- Executor (Stage 2 Stub) ----------------- */
int is_builtin(const char *name);
int run_builtin(struct command *cmd);

void execute_command(struct command *cmd) {
    while (cmd) {
        if (!cmd || cmd->argc==0) { cmd = cmd->next; continue; }

        if (is_builtin(cmd->argv[0])) {
            run_builtin(cmd);
        } else {
            /* External command stub: Stage 3 will fork/exec and handle redirection, pipes, background */
            printf("External command: %s", cmd->argv[0]);
            if (cmd->infile) printf(" < %s", cmd->infile);
            if (cmd->outfile) printf(" >%s %s", cmd->append?"":"", cmd->outfile);
            if (cmd->background) printf(" &");
            printf("\n");
        }

        cmd = cmd->next;
    }
}
