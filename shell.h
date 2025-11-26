#ifndef SHELL_H
#define SHELL_H

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>

/* ----------------- Command Struct ----------------- */
struct command {
    char **argv;
    int argc;
    int background;
    char *infile;
    char *outfile;
    int append;
    struct command *next;
};

/* ----------------- Parser ----------------- */
struct command *parse_line(const char *line);
void free_command(struct command *cmd);

/* ----------------- Executor ----------------- */
void execute_command(struct command *cmd);

/* ----------------- Built-ins ----------------- */
int is_builtin(const char *name);
int run_builtin(struct command *cmd);

/* ----------------- Shell initialization ----------------- */
void init_shell();
void cleanup_shell();

/* ----------------- Jobs & signals ----------------- */
typedef struct {
    int id;
    int running;
    char cmdline[1024];
} job_t;

void init_jobs();
void cleanup_jobs();
void add_job(const char *cmdline,int bg);
void print_jobs();
void bring_job(int id);
void runbg_job(int id);

/* ----------------- History (wrappers around readline) ----------------- */
void sh_history_init();
void sh_history_add(const char *cmd);
void sh_history_print();
void sh_history_cleanup();

/* ----------------- Signals ----------------- */
void init_signals();
void cleanup_signals();

#endif

