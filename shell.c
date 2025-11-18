#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "shell.h"

/* ---------------- helper ---------------- */
static char *strdup_safe(const char *s) {
    if (!s) return NULL;
    char *r = malloc(strlen(s)+1);
    if (r) strcpy(r, s);
    return r;
}

/* ---------------- command allocation ---------------- */
struct command *new_command() {
    struct command *c = calloc(1, sizeof(struct command));
    c->argv = NULL;
    c->argc = 0;
    c->background = 0;
    c->infile = NULL;
    c->outfile = NULL;
    c->append = 0;
    c->next = NULL;
    return c;
}

static void push_arg(struct command *c, const char *arg) {
    c->argv = realloc(c->argv, sizeof(char*)*(c->argc+2));
    c->argv[c->argc] = strdup_safe(arg);
    c->argc++;
    c->argv[c->argc] = NULL;
}

/* ---------------- PARSER: supports quotes, escapes, pipes, redir, background ---------------- */
struct command *parse_line(const char *line) {
    if (!line) return NULL;
    int len = strlen(line);
    int i = 0;
    char token[2048];
    int tpos = 0;
    char quote = 0;

    struct command *head = new_command();
    struct command *cur = head;

    while (i <= len) {
        char c = line[i];

        if ((c == ' ' || c == '\t' || c == '\0') && quote == 0) {
            if (tpos > 0) {
                token[tpos] = '\0';
                push_arg(cur, token);
                tpos = 0;
            }
            if (c == '\0') break;
            i++;
            continue;
        }

        if ((c == '"' || c == '\'') && quote == 0) {
            quote = c;
            i++;
            continue;
        } else if (c == quote) {
            quote = 0;
            i++;
            continue;
        }

        if (c == '\\') {
            i++;
            if (i <= len && line[i] != '\0') {
                token[tpos++] = line[i++];
            }
            continue;
        }

        if (c == '|') {
            if (tpos > 0) {
                token[tpos] = '\0';
                push_arg(cur, token);
                tpos = 0;
            }
            cur->next = new_command();
            cur = cur->next;
            i++;
            continue;
        }

        if (c == '<' || c == '>') {
            if (tpos > 0) {
                token[tpos] = '\0';
                push_arg(cur, token);
                tpos = 0;
            }
            int append = 0;
            if (c == '>' && line[i+1] == '>') { append = 1; i++; }
            i++;
            while (line[i] == ' ' || line[i] == '\t') i++;
            /* read filename (allow quoted) */
            char fname[1024];
            int fpos = 0;
            char fq = 0;
            if (line[i] == '"' || line[i] == '\'') {
                fq = line[i++];
                while (line[i] && line[i] != fq) {
                    if (line[i] == '\\' && line[i+1]) { i++; fname[fpos++] = line[i++]; continue; }
                    fname[fpos++] = line[i++];
                }
                if (line[i] == fq) i++;
            } else {
                while (line[i] && !isspace((unsigned char)line[i]) && line[i] != '>' && line[i] != '<' && line[i] != '|' && line[i] != '&') {
                    fname[fpos++] = line[i++];
                }
            }
            fname[fpos] = '\0';
            if (c == '<') {
                cur->infile = strdup_safe(fname);
            } else {
                cur->outfile = strdup_safe(fname);
                cur->append = append;
            }
            continue;
        }

        if (c == '&') {
            /* background: set on current command (interpreted as pipeline background if in pipeline) */
            cur->background = 1;
            i++;
            continue;
        }

        /* normal char */
        token[tpos++] = c;
        i++;
    }

    /* if head has no args and no next, free and return NULL */
    if (head->argc == 0 && head->next == NULL) {
        free_command(head);
        return NULL;
    }

    return head;
}

/* ---------------- FREE ---------------- */
void free_command(struct command *cmd) {
    while (cmd) {
        for (int i = 0; i < cmd->argc; ++i) free(cmd->argv[i]);
        free(cmd->argv);
        if (cmd->infile) free(cmd->infile);
        if (cmd->outfile) free(cmd->outfile);
        struct command *n = cmd->next;
        free(cmd);
        cmd = n;
    }
}

/* ---------------- EXECUTE: supports multiple pipes, redir, background ---------------- */
int is_builtin(const char *name);
int run_builtin(struct command *cmd);

void execute_command(struct command *cmd) {
    if (!cmd) return;

    /* count commands in pipeline */
    int n = 0;
    struct command *c = cmd;
    while (c) { n++; c = c->next; }

    /* single command and builtin -> run in parent */
    if (n == 1 && cmd->argc > 0 && is_builtin(cmd->argv[0])) {
        run_builtin(cmd);
        return;
    }

    /* create pipes: (n-1) pipes, each pipe is [read, write] */
    int (*pipes)[2] = NULL;
    if (n > 1) {
        pipes = malloc(sizeof(int[2]) * (n-1));
        for (int i = 0; i < n-1; ++i) {
            if (pipe(pipes[i]) < 0) {
                perror("pipe");
                /* cleanup allocated pipes */
                for (int j = 0; j < i; ++j) { close(pipes[j][0]); close(pipes[j][1]); }
                free(pipes);
                return;
            }
        }
    }

    pid_t *pids = malloc(sizeof(pid_t) * n);
    int idx = 0;
    c = cmd;
    while (c) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            /* cleanup */
            for (int j = 0; j < idx; ++j) waitpid(pids[j], NULL, 0);
            if (pipes) {
                for (int j = 0; j < n-1; ++j) { close(pipes[j][0]); close(pipes[j][1]); }
                free(pipes);
            }
            free(pids);
            return;
        }

        if (pid == 0) {
            /* child process */

            /* if not first command, connect stdin to previous pipe read end */
            if (idx > 0) {
                dup2(pipes[idx-1][0], STDIN_FILENO);
            } else if (c->infile) {
                /* first command and has infile */
                int fd = open(c->infile, O_RDONLY);
                if (fd < 0) { perror("open infile"); _exit(127); }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            /* if not last command, connect stdout to current pipe write end */
            if (c->next) {
                dup2(pipes[idx][1], STDOUT_FILENO);
            } else if (c->outfile) {
                int fd;
                if (c->append)
                    fd = open(c->outfile, O_WRONLY | O_CREAT | O_APPEND, 0644);
                else
                    fd = open(c->outfile, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd < 0) { perror("open outfile"); _exit(127); }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            /* close all pipe fds in child */
            if (pipes) {
                for (int j = 0; j < n-1; ++j) {
                    close(pipes[j][0]);
                    close(pipes[j][1]);
                }
            }

            /* if builtin inside pipeline -> run in child then exit */
            if (is_builtin(c->argv[0])) {
                run_builtin(c);
                _exit(0);
            }

            /* execute external */
            execvp(c->argv[0], c->argv);
            perror("execvp");
            _exit(127);
        } /* parent continues */

        pids[idx++] = pid;
        c = c->next;
    }

    /* parent: close all pipe fds */
    if (pipes) {
        for (int j = 0; j < n-1; ++j) {
            close(pipes[j][0]);
            close(pipes[j][1]);
        }
        free(pipes);
    }

    /* if background requested on last command (treat pipeline background) */
    int background = cmd->background;
    if (background) {
        /* print first child's pid as job pid */
        printf("[background] %d\n", pids[0]);
    } else {
        /* wait for all children */
        for (int j = 0; j < n; ++j) {
            int status;
            waitpid(pids[j], &status, 0);
        }
    }

    free(pids);
}
