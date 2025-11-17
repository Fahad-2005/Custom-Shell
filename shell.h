#ifndef SHELL_H
#define SHELL_H

/* ----------------- Command Struct ----------------- */
struct command {
    char **argv;            // argument list
    int argc;               // number of arguments
    int background;         // 1 if command ends with &
    char *infile;           // input redirection file
    char *outfile;          // output redirection file
    int append;             // 1 if >> append
    struct command *next;   // next command in a pipeline
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
