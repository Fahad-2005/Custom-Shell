//========utils.c=========//

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include "shell.h"

/* ---------------- HISTORY (CUSTOM FOR look) ---------------- */
#define HISTORY_MAX 500
static char *myhistory[HISTORY_MAX];
static int my_history_count = 0;

void sh_history_init() {
    my_history_count = 0;
}

void sh_history_add(const char *cmd) {
    if (!cmd || *cmd == '\0') return;

    char *copy = strdup(cmd);
    if (!copy) return;

    if (my_history_count == HISTORY_MAX) {
        free(myhistory[0]);
        memmove(myhistory, myhistory + 1, sizeof(char *) * (HISTORY_MAX - 1));
        my_history_count--;
    }

    myhistory[my_history_count++] = copy;
}

void sh_history_print() {
    for (int i = 0; i < my_history_count; i++) {
        printf("%4d  %s\n", i + 1, myhistory[i]);
    }
}

void sh_history_cleanup() {
    for (int i = 0; i < my_history_count; i++)
        free(myhistory[i]);
    my_history_count = 0;
}

/* ---------------- JOBS ---------------- */
#define JOB_MAX 50
static job_t jobs[JOB_MAX];
static int job_count = 0;

void init_jobs(){ job_count = 0; memset(jobs,0,sizeof(jobs)); }
void cleanup_jobs(){ job_count = 0; }

void add_job(const char *cmdline, int bg){
    if(job_count >= JOB_MAX) return;
    jobs[job_count].id = job_count+1;
    jobs[job_count].running = bg ? 1 : 0;
    strncpy(jobs[job_count].cmdline, cmdline, 1023);
    jobs[job_count].cmdline[1023] = 0;
    if(bg) printf("[background] %s\n", cmdline);
    job_count++;
}

void print_jobs(){
    for(int i = 0; i < job_count; i++){
        printf("[%d] %s %s\n",
            jobs[i].id,
            jobs[i].running ? "Running" : "Stopped",
            jobs[i].cmdline
        );
    }
}

void bring_job(int id){
    for(int i = 0; i < job_count; i++){
        if(jobs[i].id == id){
            jobs[i].running = 1;
            printf("[bring] Job %d brought to foreground: %s\n", id, jobs[i].cmdline);
            return;
        }
    }
    printf("bring: job %d not found\n", id);
}

void runbg_job(int id){
    for(int i = 0; i < job_count; i++){
        if(jobs[i].id == id){
            jobs[i].running = 1;
            printf("[runbg] Job %d running in background: %s\n", id, jobs[i].cmdline);
            return;
        }
    }
    printf("runbg: job %d not found\n", id);
}

/* ---------------- BUILT-INS ---------------- */
int is_builtin(const char *name){
    if(!name) return 0;
    return strcmp(name,"go")==0 || strcmp(name,"here")==0 || strcmp(name,"say")==0 ||
           strcmp(name,"make")==0 || strcmp(name,"newf")==0 || strcmp(name,"del")==0 ||
           strcmp(name,"show")==0 || strcmp(name,"edit")==0 || strcmp(name,"look")==0 ||
           strcmp(name,"tasks")==0 || strcmp(name,"bring")==0 || strcmp(name,"runbg")==0 ||
           strcmp(name,"quit")==0 || strcmp(name,"help")==0;
}

int run_builtin(struct command *cmd){
    if(!cmd || cmd->argc == 0) return 0;
    const char *name = cmd->argv[0];

    if(strcmp(name,"go")==0){ 
        if(cmd->argc<2){ printf("go: missing argument\n"); return 1; }
        if(chdir(cmd->argv[1])!=0) perror("go");
    }
    else if(strcmp(name,"here")==0){
        char cwd[512]; if(getcwd(cwd,sizeof(cwd))!=NULL) printf("%s\n",cwd);
    }
    else if(strcmp(name,"say")==0){
        for(int i = 1; i < cmd->argc; i++) printf("%s ", cmd->argv[i]);
        printf("\n");
    }
    else if(strcmp(name,"make")==0){
        for(int i = 1; i < cmd->argc; i++) 
            if(mkdir(cmd->argv[i], 0755) < 0) perror("make");
    }
    else if(strcmp(name,"newf")==0){
        for(int i = 1; i < cmd->argc; i++){
            FILE *f = fopen(cmd->argv[i], "a");
            if(f) fclose(f);
            else perror("newf");
        }
    }
    else if(strcmp(name,"del")==0){
        for(int i = 1; i < cmd->argc; i++)
            if(remove(cmd->argv[i]) != 0) perror("del");
    }
    else if(strcmp(name,"edit")==0){
        if(cmd->argc < 2){
            printf("edit: missing argument\n");
            return 1;
        }

        FILE *f = fopen(cmd->argv[1], "r+");
        if(!f) f = fopen(cmd->argv[1], "w+");
        if(!f){ perror("edit"); return 1; }

        char buf[1024];

        printf("[edit] Current file content:\n");
        while(fgets(buf, sizeof(buf), f)){
            fputs(buf, stdout);
        }

        printf("\n[edit] Add new lines. Type 'EOF' on a new line to save and exit\n");

        fseek(f, 0, SEEK_END);

        while(fgets(buf, sizeof(buf), stdin)){
            if(strcmp(buf,"EOF\n")==0) break;
            fputs(buf,f);
        }

        fclose(f);
    }
    else if(strcmp(name,"show")==0){
        printf("[show] Listing directory (emulated)\n");
    }
    else if(strcmp(name,"look")==0){
        sh_history_print();
    }
    else if(strcmp(name,"tasks")==0) print_jobs();
    else if(strcmp(name,"bring")==0){ if(cmd->argc>1) bring_job(atoi(cmd->argv[1])); }
    else if(strcmp(name,"runbg")==0){ if(cmd->argc>1) runbg_job(atoi(cmd->argv[1])); }
    else if(strcmp(name,"help")==0){
        printf("Custom built-ins: go here say make newf del show edit look tasks bring runbg quit\n");
    }
    else if(strcmp(name,"quit")==0) exit(0);
    else return 0;

    return 1;
}

/* ---------------- SIGNALS ---------------- */
static struct sigaction old_sigint;
static struct sigaction old_sigtstp;

void init_signals() {
    struct sigaction sa;
    sa.sa_handler = SIG_IGN;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;

    sigaction(SIGINT, &sa, &old_sigint);
    sigaction(SIGTSTP, &sa, &old_sigtstp);
}

void cleanup_signals() {
    sigaction(SIGINT, &old_sigint, NULL);
    sigaction(SIGTSTP, &old_sigtstp, NULL);
}

