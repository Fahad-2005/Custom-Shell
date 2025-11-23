#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <termios.h>
#include "shell.h"
#include <errno.h>


/* ----------------- HISTORY ----------------- */
#define HISTORY_MAX 500
static char *history[HISTORY_MAX];
static int history_count = 0;

void init_history() { history_count = 0; }
void add_history(const char *cmd) {
    if (!cmd) return;
    if (history_count == HISTORY_MAX) { free(history[0]); memmove(history, history+1, sizeof(char*)*(HISTORY_MAX-1)); history_count--; }
    history[history_count++] = strdup(cmd);
}
void print_history() { for(int i=0;i<history_count;i++) printf("%4d  %s\n", i+1, history[i]); }
void cleanup_history() { for(int i=0;i<history_count;i++) free(history[i]); history_count=0; }

/*---------------Builtins-----------*/

int is_builtin(const char *name) {
    if (!name) return 0;
    return strcmp(name,"cd")==0 || strcmp(name,"pwd")==0 || strcmp(name,"help")==0 ||
           strcmp(name,"history")==0 || strcmp(name,"jobs")==0 || strcmp(name,"fg")==0 ||
           strcmp(name,"bg")==0 || strcmp(name,"exit")==0;
}

int run_builtin(struct command *cmd){
    if (!cmd || cmd->argc==0) return 0;
    const char *name = cmd->argv[0];
    
    if(strcmp(name,"cd")==0){
        if(cmd->argc<2){ fprintf(stderr,"cd: missing argument\n"); return 1;}
        if(chdir(cmd->argv[1])!=0) perror("cd");
    }
    else if(strcmp(name,"pwd")==0){
        char cwd[512]; if(getcwd(cwd,sizeof(cwd))!=NULL) printf("%s\n",cwd);
    }
    else if(strcmp(name,"help")==0){
        printf("Built-ins: cd pwd help exit history jobs fg bg\n");
    }
    else if(strcmp(name,"history")==0){
        print_history();
    }
    else if(strcmp(name,"jobs")==0){
        print_jobs();
    }
    else if(strcmp(name,"fg")==0){
        if(cmd->argc<2){ fprintf(stderr,"fg: missing argument\n"); return 1;}
        int id = atoi(cmd->argv[1]); fg_job(id);
    }
    else if(strcmp(name,"bg")==0){
        if(cmd->argc<2){ fprintf(stderr,"bg: missing argument\n"); return 1;}
        int id = atoi(cmd->argv[1]); bg_job(id);
    }
    else if(strcmp(name,"exit")==0){
        exit(0);
    }
    else{
        return 0;
    }
    return 1;
}

/* ----------------- JOBS ----------------- */
#define JOB_MAX 50
static job_t jobs[JOB_MAX];
static int job_count = 0;

void init_jobs() { job_count=0; memset(jobs,0,sizeof(jobs)); }
void cleanup_jobs() { job_count=0; }

void add_job(pid_t pgid, int bg, const char *cmdline) {
    if(job_count>=JOB_MAX) return;
    jobs[job_count].id = job_count+1;
    jobs[job_count].pgid = pgid;
    jobs[job_count].running = 1;
    strncpy(jobs[job_count].cmdline, cmdline, 1023);
    jobs[job_count].cmdline[1023]=0;
    if(bg) printf("[background] %d\n", pgid);
    job_count++;
}

void update_job_status() {
    int status;
    pid_t pid;
    for(int i=0;i<job_count;i++){
        if(jobs[i].running){
            pid = waitpid(-jobs[i].pgid, &status, WNOHANG|WUNTRACED|WCONTINUED);
            if(pid>0){
                if(WIFSTOPPED(status)) jobs[i].running=0;
                else if(WIFEXITED(status) || WIFSIGNALED(status)) jobs[i].running=0;
            }
        }
    }
}

void print_jobs(){
    update_job_status();
    for(int i=0;i<job_count;i++){
        printf("[%d] %s %s (pgid=%d)\n",
               jobs[i].id,
               jobs[i].running?"Running":"Stopped",
               jobs[i].cmdline,
               jobs[i].pgid);
    }
}

void fg_job(int id) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].id == id) {
            if (!jobs[i].running) kill(-jobs[i].pgid, SIGCONT);
            jobs[i].running = 1;

            // Give terminal control
            tcsetpgrp(STDIN_FILENO, jobs[i].pgid);
            int status;
            waitpid(-jobs[i].pgid, &status, WUNTRACED);
            tcsetpgrp(STDIN_FILENO, getpgrp());
            return;
        }
    }
    printf("fg: job %d not found\n", id);
}

void bg_job(int id) {
    for (int i = 0; i < job_count; i++) {
        if (jobs[i].id == id) {
            kill(-jobs[i].pgid, SIGCONT);
            jobs[i].running = 1;
            return;
        }
    }
    printf("bg: job %d not found\n", id);
}



/* ----------------- SIGNALS ----------------- */
void init_signals() {
    signal(SIGTTOU,SIG_IGN);
    signal(SIGTTIN,SIG_IGN);
    signal(SIGCHLD,SIG_IGN);
}
void cleanup_signals() {}

