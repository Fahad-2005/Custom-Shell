#ifndef SHELL_H
#define SHELL_H

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

/* ----------------- Job & signal stubs ----------------- */
void init_jobs();
void cleanup_jobs();
void init_signals();
void cleanup_signals();

/* ----------------- History stubs ----------------- */
void init_history();
void add_history(const char *cmd);
void print_history();

#endif
