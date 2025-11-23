#ifndef SHELL_H
#define SHELL_H

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

/* ----------------- Job & signal ----------------- */
typedef struct {
    int id;
    pid_t pgid;
    int running;        // 1 = running, 0 = stopped
    char cmdline[1024];
} job_t;

void init_jobs();
void cleanup_jobs();
void add_job(pid_t pgid, int bg, const char *cmdline);
void print_jobs();
void fg_job(int id);
void bg_job(int id);

/* ----------------- History ----------------- */
void init_history();
void add_history(const char *cmd);
void print_history();
void cleanup_history();

/* ----------------- Signals ----------------- */
void init_signals();
void cleanup_signals();

#endif

