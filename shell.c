//===========shell.c============//

#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
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
    c->argv = realloc(c->argv,sizeof(char*)*(c->argc+2));
    c->argv[c->argc] = strdup_safe(arg);
    c->argc++;
    c->argv[c->argc] = NULL;
}

/* ---------------- PARSER ---------------- */
struct command *parse_line(const char *line) {
    if (!line) return NULL;
    int len = strlen(line), i=0, tpos=0;
    char token[2048], quote=0;
    struct command *head = new_command();
    struct command *cur = head;

    while(i<=len){
        char c = line[i];
        if((c==' '||c=='\t'||c=='\0') && quote==0){
            if(tpos>0){ token[tpos]='\0'; push_arg(cur,token); tpos=0; }
            if(c=='\0') break;
            i++; continue;
        }
        if((c=='"'||c=='\'') && quote==0){ quote=c; i++; continue; }
        else if(c==quote){ quote=0; i++; continue; }
        if(c=='\\'){ i++; if(i<=len && line[i]!='\0') token[tpos++]=line[i++]; continue; }
        if(c=='|'){ if(tpos>0){ token[tpos]='\0'; push_arg(cur,token); tpos=0;} cur->next=new_command(); cur=cur->next; i++; continue; }
        if(c=='<'||c=='>'){
            if(tpos>0){ token[tpos]='\0'; push_arg(cur,token); tpos=0;}
            int append=0;
            if(c=='>' && line[i+1]=='>'){ append=1; i++; }
            i++;
            while(line[i]==' '||line[i]=='\t') i++;
            char fname[1024]; int fpos=0; char fq=0;
            if(line[i]=='"'||line[i]=='\''){ fq=line[i++]; while(line[i] && line[i]!=fq){ if(line[i]=='\\' && line[i+1]){i++; fname[fpos++]=line[i++]; continue;} fname[fpos++]=line[i++];} if(line[i]==fq)i++; }
            else { while(line[i] && !isspace(line[i]) && line[i]!='>' && line[i]!='<' && line[i]!='|' && line[i]!='&'){ fname[fpos++]=line[i++]; } }
            fname[fpos]='\0';
            if(c=='<') cur->infile=strdup_safe(fname);
            else { cur->outfile=strdup_safe(fname); cur->append=append; }
            continue;
        }
        if(c=='&'){ cur->background=1; i++; continue; }
        token[tpos++]=c; i++;
    }
    if(head->argc==0 && head->next==NULL){ free_command(head); return NULL; }
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

/* ---------------- BUILTIN FUNCTIONS FOR PIPE SUPPORT ---------------- */
void say_builtin(struct command *cmd, int out_fd){
    for(int i=1;i<cmd->argc;i++)
        dprintf(out_fd,"%s ",cmd->argv[i]);
    dprintf(out_fd,"\n");
}

void here_builtin(int out_fd){
    char cwd[512];
    if(getcwd(cwd,sizeof(cwd))!=NULL)
        dprintf(out_fd,"%s\n",cwd);
}

/* ---------------- EXECUTE ---------------- */
extern int is_builtin(const char *name);
extern int run_builtin(struct command *cmd);
extern void add_job(const char *cmdline,int bg);

void execute_command(struct command *cmd){
    if(!cmd) return;

    // Run built-ins that must affect the parent shell
    if(cmd->argc>0){
        if(strcmp(cmd->argv[0],"go")==0 || strcmp(cmd->argv[0],"quit")==0){
            run_builtin(cmd);
            return;
        }
    }

    struct command *c = cmd;
    int in_fd = STDIN_FILENO;

    while(c){
        int fd[2] = {0,0};
        if(c->next) pipe(fd);

        pid_t pid = fork();
        if(pid==0){
            // CHILD PROCESS - Reset signal handlers to default
            signal(SIGINT, SIG_DFL);
            signal(SIGTSTP, SIG_DFL);
            
            if(in_fd != STDIN_FILENO){
                dup2(in_fd, STDIN_FILENO);
                close(in_fd);
            }
            if(c->next){
                close(fd[0]);
                dup2(fd[1], STDOUT_FILENO);
                close(fd[1]);
            }
            if(c->infile){
                FILE *f = fopen(c->infile,"r");
                if(f){ dup2(fileno(f),STDIN_FILENO); fclose(f); }
            }
            if(c->outfile){
                FILE *f;
                if(c->append) f=fopen(c->outfile,"a");
                else f=fopen(c->outfile,"w");
                if(f){ dup2(fileno(f),STDOUT_FILENO); fclose(f); }
            }

            if(c->argc>0 && is_builtin(c->argv[0])){
                if(strcmp(c->argv[0],"say")==0) say_builtin(c, STDOUT_FILENO);
                else if(strcmp(c->argv[0],"here")==0) here_builtin(STDOUT_FILENO);
                else run_builtin(c);
                _exit(0);
            }

            execvp(c->argv[0],c->argv);
            perror("exec");
            _exit(127);
        }
        else if(pid>0){
            // PARENT PROCESS
            if(in_fd != STDIN_FILENO) close(in_fd);
            if(c->next){
                close(fd[1]);
                in_fd = fd[0]; // next command reads from here
            }
            
            if(!c->background){
                int status;
                waitpid(pid, &status, 0);
            }
            else {
                add_job(c->argv[0], 1);
            }
        }
        else {
            perror("fork");
        }

        c = c->next;
    }
}

/* ---------------- INIT / CLEANUP ---------------- */
extern void sh_history_init();
extern void sh_history_cleanup();
extern void init_jobs();
extern void init_signals();
extern void cleanup_jobs();
extern void cleanup_signals();

void init_shell(){ sh_history_init(); init_jobs(); init_signals(); }
void cleanup_shell(){ cleanup_jobs(); cleanup_signals(); sh_history_cleanup(); }

