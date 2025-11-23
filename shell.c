#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <signal.h>
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

/* ---------------- PARSER ---------------- */
struct command *parse_line(const char *line) {
    if (!line) return NULL;
    int len = strlen(line);
    int i = 0, tpos = 0;
    char token[2048];
    char quote = 0;
    struct command *head = new_command();
    struct command *cur = head;

    while (i <= len) {
        char c = line[i];
        if ((c==' '||c=='\t'||c=='\0') && quote==0) {
            if(tpos>0){ token[tpos]='\0'; push_arg(cur, token); tpos=0; }
            if(c=='\0') break;
            i++; continue;
        }
        if((c=='"'||c=='\'') && quote==0){ quote=c; i++; continue;}
        else if(c==quote){ quote=0; i++; continue;}
        if(c=='\\'){ i++; if(i<=len && line[i]!='\0') token[tpos++]=line[i++]; continue; }
        if(c=='|'){ if(tpos>0){ token[tpos]='\0'; push_arg(cur,token); tpos=0;} cur->next=new_command(); cur=cur->next; i++; continue; }
        if(c=='<'||c=='>'){
            if(tpos>0){ token[tpos]='\0'; push_arg(cur,token); tpos=0;}
            int append=0;
            if(c=='>' && line[i+1]=='>'){ append=1; i++; }
            i++;
            while(line[i]==' '||line[i]=='\t') i++;
            char fname[1024]; int fpos=0; char fq=0;
            if(line[i]=='"'||line[i]=='\''){ fq=line[i++]; while(line[i] && line[i]!=fq){ if(line[i]=='\\' && line[i+1]){i++; fname[fpos++]=line[i++]; continue;} fname[fpos++]=line[i++];} if(line[i]==fq)i++;}
            else { while(line[i] && !isspace(line[i]) && line[i]!='>' && line[i]!='<' && line[i]!='|' && line[i]!='&'){ fname[fpos++]=line[i++];}}
            fname[fpos]='\0';
            if(c=='<') cur->infile=strdup_safe(fname);
            else { cur->outfile=strdup_safe(fname); cur->append=append;}
            continue;
        }
        if(c=='&'){ cur->background=1; i++; continue; }
        token[tpos++]=c; i++;
    }
    if(head->argc==0 && head->next==NULL){ free_command(head); return NULL;}
    return head;
}

/* ---------------- FREE ---------------- */
void free_command(struct command *cmd){
    while(cmd){
        for(int i=0;i<cmd->argc;i++) free(cmd->argv[i]);
        free(cmd->argv);
        if(cmd->infile) free(cmd->infile);
        if(cmd->outfile) free(cmd->outfile);
        struct command *n=cmd->next;
        free(cmd);
        cmd=n;
    }
}

/* ---------------- EXECUTE ---------------- */
extern int is_builtin(const char *name);
extern int run_builtin(struct command *cmd);

void execute_command(struct command *cmd) {
    if (!cmd) return;

    int n = 0;
    struct command *c = cmd;
    while (c) { n++; c = c->next; }

    if (n == 1 && cmd->argc > 0 && is_builtin(cmd->argv[0])) {
        run_builtin(cmd);
        return;
    }

    int (*pipes)[2] = NULL;
    if (n > 1) {
        pipes = malloc(sizeof(int[2]) * (n - 1));
        for (int i = 0; i < n - 1; i++) {
            if (pipe(pipes[i]) < 0) { perror("pipe"); return; }
        }
    }

    pid_t *pids = malloc(sizeof(pid_t) * n);
    pid_t pgid = 0;
    int idx = 0;
    c = cmd;

    while (c) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return; }

        if (pid == 0) { // child
            if (idx == 0) pgid = getpid();
            setpgid(0, pgid);

            if (!cmd->background) {
                signal(SIGTTOU, SIG_DFL);
                signal(SIGTTIN, SIG_DFL);
            }

            if (idx > 0) dup2(pipes[idx - 1][0], STDIN_FILENO);
            if (c->next) dup2(pipes[idx][1], STDOUT_FILENO);

            if (pipes) for (int j = 0; j < n - 1; j++) { close(pipes[j][0]); close(pipes[j][1]); }

            if (is_builtin(c->argv[0])) { run_builtin(c); _exit(0); }

            execvp(c->argv[0], c->argv);
            perror("execvp"); _exit(127);
        }

        if (idx == 0) pgid = pid;
        setpgid(pid, pgid);
        pids[idx++] = pid;
        c = c->next;
    }

    if (pipes) { for (int j = 0; j < n - 1; j++) { close(pipes[j][0]); close(pipes[j][1]); } free(pipes); }

    char full_cmd[1024] = "";
    c = cmd;
    while (c) {
        strcat(full_cmd, c->argv[0]);
        for (int i = 1; i < c->argc; i++) { strcat(full_cmd, " "); strcat(full_cmd, c->argv[i]); }
        if (c->next) strcat(full_cmd, " | ");
        c = c->next;
    }

    if (cmd->background) {
        add_job(pgid, 1, full_cmd);
    } else {
        tcsetpgrp(STDIN_FILENO, pgid);
        for (int j = 0; j < n; j++) waitpid(pids[j], NULL, WUNTRACED);
        tcsetpgrp(STDIN_FILENO, getpgrp());
    }

    free(pids);
}



/* ---------------- INIT / CLEANUP ---------------- */
extern void init_history();
extern void init_jobs();
extern void init_signals();
extern void cleanup_history();
extern void cleanup_jobs();
extern void cleanup_signals();

void init_shell(){
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    init_history();
    init_jobs();
    init_signals();
}

void cleanup_shell(){
    cleanup_jobs();
    cleanup_signals();
    cleanup_history();
}

