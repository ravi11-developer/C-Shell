#include "../include/A1.h"
#include "../include/tokenize.h"
#include "../include/shell.h"
#include "../include/A3.h"
#include "../include/ast.h"
#include "../include/B1.h"
#include <sys/wait.h>
#include "../include/B2.h"
#include "../include/shell.h"
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
    void ping_command(const char *pid_str, const char *sig_str);
    void cleanup_bg_jobs(void);
    void cleanup_bgjobs(void);
#define MAX_BG_JOBS 128
typedef struct {
    int job_num;
    pid_t pid;
    char cmd_name[256];
    char cmd_line[1024];
    int active;
    int stopped;
} BgJob;
BgJob bg_jobs[MAX_BG_JOBS];
int bg_job_count = 0;
int next_job_num = 1;
char* tokens[MAX_TOKENS];
int token_cnt;
char home[1024];
char shellcwd[1024];
char prevcwd[1024];
int prev_set = 0;
int child_pid = -1;

static void report_exec_error(const char *cmd) {
    switch (errno) {
        case ENOENT:
        case ENOTDIR:
            fprintf(stderr, "Command not found!\n");
            break;
        case EACCES:
            fprintf(stderr, "Permission denied!\n");
            break;
        case ENOEXEC:
            fprintf(stderr, "Exec format error!\n");
            break;
        default:
            perror(cmd ? cmd : "execvp");
            break;
    }
}

void check_bg_jobs() {
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active) {
            int status;
            pid_t result = waitpid(bg_jobs[i].pid, &status, WNOHANG);
            if (result > 0) {
                bg_jobs[i].active = 0;
                bg_jobs[i].stopped = 0;
                if (WIFEXITED(status)) {
                    printf("%s with pid %d exited normally\n", bg_jobs[i].cmd_name, bg_jobs[i].pid);
                } else {
                    printf("%s with pid %d exited abnormally\n", bg_jobs[i].cmd_name, bg_jobs[i].pid);
                }
            } else if (result == -1) {
                if (errno == ECHILD || errno == ESRCH) {
                    bg_jobs[i].active = 0;
                    bg_jobs[i].stopped = 0;
                }
            } else {
                if (kill(bg_jobs[i].pid, 0) == -1 && errno == ESRCH) {
                    bg_jobs[i].active = 0;
                    bg_jobs[i].stopped = 0;
                }
            }
        }
    }
    cleanup_bg_jobs();
}
void print_prompt() {
    char res[1024];
    getcwd(shellcwd, sizeof(shellcwd));
    computername(res, shellcwd);
    printf("<%s> ", res);
    fflush(stdout);
}

void sigint_handler(int sig) {
    if (child_pid > 0) {
        kill(-child_pid, SIGINT);
    } else {
        printf("\n");
        fflush(stdout);
    }
    signal(SIGINT, sigint_handler);
}
void execute(Command* ast) {
    int prev_fd = -1;
    int mainin = dup(STDIN_FILENO);
    int mainout = dup(STDOUT_FILENO);

    for (Command* cmd = ast; cmd != NULL; cmd = cmd->next) {
        if (!cmd->args || !cmd->args[0]) continue;
        if (strcmp(cmd->args[0], "|") == 0) continue;

        int pipefd[2];
        int usepipe = (cmd->next && strcmp(cmd->next->args[0], "|") == 0);
        if (usepipe) {
            if (pipe(pipefd) < 0) {
                perror("pipe");
                continue;
            }
        }

        if (!usepipe && prev_fd == -1) {
            int save_in = dup(STDIN_FILENO);
            int save_out = dup(STDOUT_FILENO);
            int redir_failed = 0;

            for (int i = 1; cmd->args[i] != NULL; i++) {
                if (strncmp(cmd->args[i], "<", 1) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 1) ? cmd->args[i + 1] : cmd->args[i] + 1;
                    int fd = open(path, O_RDONLY, 0644);
                    if (fd < 0) { fprintf(stderr, "No such file or directory\n"); redir_failed = 1; break; }
                    dup2(fd, STDIN_FILENO); close(fd); cmd->args[i] = NULL;
                } else if (strncmp(cmd->args[i], ">>", 2) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 2) ? cmd->args[i + 1] : cmd->args[i] + 2;
                    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); redir_failed = 1; break; }
                    dup2(fd, STDOUT_FILENO); close(fd); cmd->args[i] = NULL;
                } else if (strncmp(cmd->args[i], ">", 1) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 1) ? cmd->args[i + 1] : cmd->args[i] + 1;
                    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); redir_failed = 1; break; }
                    dup2(fd, STDOUT_FILENO); close(fd); cmd->args[i] = NULL;
                }
            }

            if (redir_failed) {
                dup2(save_in, STDIN_FILENO);
                dup2(save_out, STDOUT_FILENO);
                close(save_in); close(save_out);
                continue;
            }

            if (strcmp(cmd->name, "hop") == 0) {
                int hop_to_prev = 0;
                for (int h = 1; cmd->args[h] != NULL; h++) {
                    if (strcmp(cmd->args[h], "-") == 0) hop_to_prev = 1;
                }
                if (hop_to_prev && (prevcwd[0] == '\0' || strcmp(prevcwd, ".") == 0)) {
                } else {
                    hop(cmd);
                }
                dup2(save_in, STDIN_FILENO);
                dup2(save_out, STDOUT_FILENO);
                close(save_in); close(save_out);
                continue;
            }
            if (strcmp(cmd->name, "reveal") == 0) {
                reveal(cmd);
                dup2(save_in, STDIN_FILENO);
                dup2(save_out, STDOUT_FILENO);
                close(save_in); close(save_out);
                continue;
            }
            if (strcmp(cmd->args[0], "log") == 0) {
                char logpath[1050]; snprintf(logpath, sizeof(logpath), "log.txt");
                FILE* fd = fopen(logpath, "r");
                if (!fd) { dup2(save_in, STDIN_FILENO); dup2(save_out, STDOUT_FILENO); close(save_in); close(save_out); continue; }
                char logbuffer[16][1050]; int ln = 0;
                while (ln < 16 && fgets(logbuffer[ln], sizeof(logbuffer[ln]), fd)) ln++;
                fclose(fd);
                if (cmd->args[1] == NULL) {
                    for (int j = 0; j < ln; j++) printf("%s", logbuffer[j]);
                    dup2(save_in, STDIN_FILENO); dup2(save_out, STDOUT_FILENO); close(save_in); close(save_out);
                    continue;
                }
                for (int j = 1; cmd->args[j] != NULL; j++) {
                    if (strcmp(cmd->args[j], "purge") == 0) { int tmp = open(logpath, O_CREAT | O_WRONLY | O_TRUNC, 0644); if (tmp >= 0) close(tmp); }
                    if (strcmp(cmd->args[j], "execute") == 0 && cmd->args[j+1]) {
                        int idx = atoi(cmd->args[j+1]);
                        if (idx > 0 && idx <= ln) {
                            tokenize_input(logbuffer[ln - idx]);
                            Command* sub = parse_tokens();
                            execute(sub);
                        }
                    }
                }
                dup2(save_in, STDIN_FILENO); dup2(save_out, STDOUT_FILENO); close(save_in); close(save_out);
                continue;
            }

            pid_t cpid = fork();
            if (cpid == 0) {
                execvp(cmd->args[0], cmd->args);
                report_exec_error(cmd->args[0]);
                int code = (errno == ENOENT || errno == ENOTDIR) ? 127 : (errno == EACCES ? 126 : 1);
                _exit(code);
            } else if (cpid > 0) {
                waitpid(cpid, NULL, 0);
            } else {
                perror("fork");
            }

            dup2(save_in, STDIN_FILENO);
            dup2(save_out, STDOUT_FILENO);
            close(save_in); close(save_out);
            continue;
        }

        pid_t pid = fork();
        if (pid == 0) {
            if (prev_fd != -1) { dup2(prev_fd, STDIN_FILENO); close(prev_fd); }
            if (usepipe) { dup2(pipefd[1], STDOUT_FILENO); close(pipefd[0]); close(pipefd[1]); }

            for (int i = 1; cmd->args[i] != NULL; i++) {
                if (strncmp(cmd->args[i], "<", 1) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 1) ? cmd->args[i + 1] : cmd->args[i] + 1;
                    int fd = open(path, O_RDONLY, 0644);
                    if (fd < 0) { fprintf(stderr, "No such file or directory\n"); _exit(1); }
                    dup2(fd, STDIN_FILENO); close(fd); cmd->args[i] = NULL;
                } else if (strncmp(cmd->args[i], ">>", 2) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 2) ? cmd->args[i + 1] : cmd->args[i] + 2;
                    int fd = open(path, O_WRONLY | O_CREAT | O_APPEND, 0644);
                    if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); _exit(1); }
                    dup2(fd, STDOUT_FILENO); close(fd); cmd->args[i] = NULL;
                } else if (strncmp(cmd->args[i], ">", 1) == 0) {
                    const char *path = (strlen(cmd->args[i]) == 1) ? cmd->args[i + 1] : cmd->args[i] + 1;
                    int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd < 0) { fprintf(stderr, "Unable to create file for writing\n"); _exit(1); }
                    dup2(fd, STDOUT_FILENO); close(fd); cmd->args[i] = NULL;
                }
            }

            if (strcmp(cmd->args[0], "reveal") == 0) { reveal(cmd); _exit(0); }
            if (strcmp(cmd->args[0], "hop") == 0) { _exit(1); }
            if (strcmp(cmd->args[0], "log") == 0) {
                char logpath[1050]; snprintf(logpath, sizeof(logpath), "log.txt");
                FILE* fd = fopen(logpath, "r");
                if (fd) {
                    char logbuffer[16][1050]; int ln = 0;
                    while (ln < 16 && fgets(logbuffer[ln], sizeof(logbuffer[ln]), fd)) ln++;
                    fclose(fd);
                    if (cmd->args[1] == NULL) {
                        for (int j = 0; j < ln; j++) printf("%s", logbuffer[j]);
                    } else {
                        for (int j = 1; cmd->args[j] != NULL; j++) {
                            if (strcmp(cmd->args[j], "purge") == 0) { int tmp = open(logpath, O_CREAT | O_WRONLY | O_TRUNC, 0644); if (tmp >= 0) close(tmp); }
                            if (strcmp(cmd->args[j], "execute") == 0 && cmd->args[j+1]) {
                                int idx = atoi(cmd->args[j+1]);
                                if (idx > 0 && idx <= ln) {
                                    tokenize_input(logbuffer[ln - idx]);
                                    Command* sub = parse_tokens();
                                    execute(sub);
                                }
                            }
                        }
                    }
                }
                _exit(0);
            }

            execvp(cmd->args[0], cmd->args);
            report_exec_error(cmd->args[0]);
            {
                int code = (errno == ENOENT || errno == ENOTDIR) ? 127 : (errno == EACCES ? 126 : 1);
                _exit(code);
            }
        } else if (pid > 0) {
            int status = 0;
            waitpid(pid, &status, 0);
            if (prev_fd != -1) { close(prev_fd); }
            if (usepipe) {
                close(pipefd[1]);
                prev_fd = pipefd[0];
            } else {
                prev_fd = -1;
            }
        } else {
            perror("fork");
            if (usepipe) { close(pipefd[0]); close(pipefd[1]); }
        }
    }

    if (prev_fd != -1) close(prev_fd);
    dup2(mainin, STDIN_FILENO);
    dup2(mainout, STDOUT_FILENO);
    close(mainin); close(mainout);
}
int cmp_bgjob(const void *a, const void *b) {
    const BgJob *ja = *(const BgJob **)a;
    const BgJob *jb = *(const BgJob **)b;
    return strcmp(ja->cmd_name, jb->cmd_name);
}
const char* get_job_state(pid_t pid) {
    char proc_path[64];
    snprintf(proc_path, sizeof(proc_path), "/proc/%d/stat", pid);
    FILE *fp = fopen(proc_path, "r");
    if (!fp) return "Stopped";

    int pid_read = 0;
    char comm[256] = {0};
    char state = 'R';
    if (fscanf(fp, "%d (%255[^)]) %c", &pid_read, comm, &state) != 3) {
        fclose(fp);
        return "Stopped";
    }
    fclose(fp);

    if (state == 'T') return "Stopped";
    return "Running";
}

void cleanup_bg_jobs() {
    int j = 0;
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active) {
            bg_jobs[j++] = bg_jobs[i];
        }
    }
    bg_job_count = j;
}
void cleanup_bgjobs() {
    cleanup_bg_jobs();
}

void activities() {
    for (int i = 0; i < bg_job_count; ++i) {
        if (!bg_jobs[i].active) continue;

        int status;
        pid_t result = waitpid(bg_jobs[i].pid, &status, WNOHANG);
        if (result > 0) {
            bg_jobs[i].active = 0;
            bg_jobs[i].stopped = 0;
        } else if (result == -1) {
            if (errno == ECHILD || errno == ESRCH) {
                bg_jobs[i].active = 0;
                bg_jobs[i].stopped = 0;
            }
        } else {
            if (kill(bg_jobs[i].pid, 0) == -1 && errno == ESRCH) {
                bg_jobs[i].active = 0;
                bg_jobs[i].stopped = 0;
            }
        }
    }
    cleanup_bg_jobs();

    BgJob* job_ptrs[MAX_BG_JOBS];
    int count = 0;
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active) {
            job_ptrs[count++] = &bg_jobs[i];
        }
    }
    qsort(job_ptrs, count, sizeof(BgJob*), cmp_bgjob);

    for (int i = 0; i < count; ++i) {
        const char* state;
        if (job_ptrs[i]->stopped) {
            state = "Stopped";
        } else {
            state = get_job_state(job_ptrs[i]->pid);
        }
        printf("[%d] : %s - %s\n", job_ptrs[i]->pid, job_ptrs[i]->cmd_name, state);
    }
}

volatile sig_atomic_t fg_stopped = 0;
volatile sig_atomic_t fg_pid = -1;
char fg_cmd_name[256] = "";
char fg_cmd_line[1024] = "";

static void build_cmd_line_from_args(char *dst, size_t n, char **args) {
    if (!dst || n == 0) return;
    dst[0] = '\0';
    if (!args) return;
    size_t left = n;
    for (int i = 0; args[i]; i++) {
        if (i) { strncat(dst, " ", left > 0 ? left - 1 : 0); }
        strncat(dst, args[i], left > 0 ? left - 1 : 0);
        left = n - strlen(dst);
        if (left <= 1) break;
    }
}

static int find_job_index_by_num(int job_num) {
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active && bg_jobs[i].job_num == job_num) return i;
    }
    return -1;
}

static int find_mru_job_index(void) {
    int idx = -1;
    int best_job_num = -1;
    for (int i = 0; i < bg_job_count; ++i) {
        if (bg_jobs[i].active && bg_jobs[i].job_num > best_job_num) {
            best_job_num = bg_jobs[i].job_num;
            idx = i;
        }
    }
    return idx;
}

static int is_number_str(const char *s) {
    if (!s || !*s) return 0;
    for (const char *p = s; *p; ++p) if (*p < '0' || *p > '9') return 0;
    return 1;
}

static void fg_builtin(int have_arg, int job_num) {
    int idx = have_arg ? find_job_index_by_num(job_num) : find_mru_job_index();
    if (idx == -1) { printf("No such job\n"); return; }
    BgJob *job = &bg_jobs[idx];

    const char *print_cmd = job->cmd_line[0] ? job->cmd_line : job->cmd_name;
    printf("%s\n", print_cmd);

    if (job->stopped) {
        if (kill(-job->pid, SIGCONT) == -1) {
            if (errno == ESRCH) { printf("No such job\n"); job->active = 0; job->stopped = 0; cleanup_bg_jobs(); return; }
        } else {
            job->stopped = 0;
        }
    }

    child_pid = job->pid;
    int status;
    while (1) {
        pid_t w = waitpid(job->pid, &status, WUNTRACED);
        if (w == -1) {
            if (errno == EINTR) continue;
            if (errno == ECHILD || errno == ESRCH) { job->active = 0; job->stopped = 0; }
            break;
        }
        if (WIFSTOPPED(status)) { job->stopped = 1; job->active = 1; break; }
        if (WIFEXITED(status) || WIFSIGNALED(status)) { job->active = 0; job->stopped = 0; break; }
    }
    child_pid = -1;
    cleanup_bg_jobs();
}

static void bg_builtin(int have_arg, int job_num) {
    int idx = have_arg ? find_job_index_by_num(job_num) : find_mru_job_index();
    if (idx == -1) { printf("No such job\n"); return; }
    BgJob *job = &bg_jobs[idx];

    if (!job->stopped) {
        const char *st = get_job_state(job->pid);
        if (strcmp(st, "Stopped") != 0) { printf("Job already running\n"); return; }
    }

    if (kill(-job->pid, SIGCONT) == -1) {
        if (errno == ESRCH) { printf("No such job\n"); }
        else perror("bg SIGCONT");
        return;
    }
    job->stopped = 0;
    const char *name_for_bg = job->cmd_name[0] ? job->cmd_name : (job->cmd_line[0] ? job->cmd_line : "job");
    printf("[%d] %s\n", job->job_num, name_for_bg);
}

void sigtstp_handler(int sig) {
    if (child_pid > 0) {
        kill(-child_pid, SIGTSTP);
        fg_stopped = 1;
        fg_pid = child_pid;
    }
    signal(SIGTSTP, sigtstp_handler);
}

int main(){

        if (signal(SIGINT, sigint_handler) == SIG_ERR) {
            perror("signal");
            exit(1);
        }
    if (signal(SIGTSTP, sigtstp_handler) == SIG_ERR) {
        perror("signal SIGTSTP");
        exit(1);
    }
    signal(SIGTTIN, SIG_IGN);
    signal(SIGTTOU, SIG_IGN);
    signal(SIGQUIT, SIG_IGN);
    if (getcwd(home, sizeof(home)) == NULL) {
        strcpy(home, ".");
    }
    if (getcwd(prevcwd, sizeof(prevcwd)) == NULL) {
        strcpy(prevcwd, ".");
    }
    strcpy(shellcwd, home);
        while(1){
    print_prompt();
        char intake[1050];
        if (fgets(intake, sizeof(intake), stdin) == NULL) {
            if (feof(stdin)) {
                if (child_pid <= 0) {
                    for (int j = 0; j < bg_job_count; ++j) {
                        if (bg_jobs[j].active) {
                            kill(bg_jobs[j].pid, SIGKILL);
                        }
                    }
                    printf("logout\n");
                    exit(0);
                }
                clearerr(stdin);
                continue;
            }
            clearerr(stdin);
            continue;
        }
        intake[strcspn(intake, "\n")] = 0;
        int non_space = 0;
        for (int k = 0; intake[k] != '\0'; k++) {
            if (intake[k] != ' ' && intake[k] != '\t') {
                non_space = 1;
                break;
            }
        }
        if (!non_space) {
            continue;
        }
        
        char logpath[1050];
        snprintf(logpath,sizeof(logpath),"log.txt");
        FILE* fp = fopen(logpath,"r");
        char logbuffer[16][1024];
        int i=0;
        if (fp) {
            while(i < 16 && fgets(logbuffer[i], sizeof(logbuffer[i]), fp) != NULL) {
                i++;
            }
            fclose(fp);
        }
        char another[1024];
        strcpy(another,intake);
        int var=strlen(another);
        another[var]='\n';
        another[var+1]='\0';

        token_cnt=tokenize_input(intake);
        Command* ast=parse_tokens();
        int fl=0;
        for(Command* cmd=ast;cmd!=NULL;cmd=cmd->next){
            if(strcmp(cmd->args[0],"log")==0){
                fl=1;
                break;
            }
        }
        if(!fl){
        if(i == 0 || strcmp(another,logbuffer[i-1])!=0){
            if(i<15){
                int fd=open(logpath,O_CREAT|O_WRONLY|O_APPEND,0644);
                write(fd,another,strlen(another));
                close(fd);
            }else{
                int fd=open(logpath,O_CREAT|O_WRONLY|O_TRUNC,0644);
                for(int j=1;j<15;j++){
                    write(fd,logbuffer[j],strlen(logbuffer[j]));
                }
                write(fd,another,strlen(another));
                close(fd);
            }
        }
    }
        if(check()){
        }else{
            printf("Invalid Syntax!\n");
            continue;
        }
        check_bg_jobs();

        if (token_cnt == 1 && strcmp(tokens[0], "activities") == 0) {
            activities();
            continue;
        }

        if (token_cnt == 3 && strcmp(tokens[0], "ping") == 0) {
            ping_command(tokens[1], tokens[2]);
            continue;
        }
        if ((token_cnt == 1 || token_cnt == 2) && strcmp(tokens[0], "fg") == 0) {
            int have_arg = (token_cnt == 2);
            if (have_arg && !is_number_str(tokens[1])) { printf("No such job\n"); continue; }
            int job_num = have_arg ? atoi(tokens[1]) : 0;
            fg_builtin(have_arg, job_num);
            continue;
        }
        if ((token_cnt == 1 || token_cnt == 2) && strcmp(tokens[0], "bg") == 0) {
            int have_arg = (token_cnt == 2);
            if (have_arg && !is_number_str(tokens[1])) { printf("No such job\n"); continue; }
            int job_num = have_arg ? atoi(tokens[1]) : 0;
            bg_builtin(have_arg, job_num);
            continue;
        }
        Command* current_cmd = ast;
        while (current_cmd != NULL) {
            if (current_cmd->is_background) {
                pid_t bg_pid = fork();
                if (bg_pid == 0) {
                    setpgid(0, 0);
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                    signal(SIGTTIN, SIG_DFL);
                    signal(SIGTTOU, SIG_DFL);
                    signal(SIGCHLD, SIG_DFL);
                    int devnull = open("/dev/null", O_RDONLY);
                    dup2(devnull, STDIN_FILENO);
                    close(devnull);
                    Command single_cmd = *current_cmd;
                    single_cmd.next = NULL;
                    execute(&single_cmd);
                    exit(0);
                } else if (bg_pid > 0) {
                    setpgid(bg_pid, bg_pid);
                    if (bg_job_count < MAX_BG_JOBS) {
                        bg_jobs[bg_job_count].job_num = next_job_num++;
                        bg_jobs[bg_job_count].pid = bg_pid;
                        strncpy(bg_jobs[bg_job_count].cmd_name, current_cmd->args[0], 255);
                        bg_jobs[bg_job_count].cmd_name[255] = '\0';
                        build_cmd_line_from_args(bg_jobs[bg_job_count].cmd_line, sizeof(bg_jobs[bg_job_count].cmd_line), current_cmd->args);
                        bg_jobs[bg_job_count].active = 1;
                        bg_jobs[bg_job_count].stopped = 0;
                        printf("[%d] %d\n", bg_jobs[bg_job_count].job_num, bg_pid);
                        bg_job_count++;
                    }
                } else {
                    perror("fork");
                }
                current_cmd = current_cmd->next;
            } else {
                Command* fg_start = current_cmd;
                Command* fg_end = current_cmd;
                while (fg_end->next && !fg_end->next->is_background) {
                    fg_end = fg_end->next;
                }
                Command* next_after_fg = fg_end->next;
                fg_end->next = NULL;
                strncpy(fg_cmd_name, fg_start->args[0], 255);
                fg_cmd_name[255] = '\0';

                if (fg_start == fg_end && strcmp(fg_start->name, "hop") == 0) {
                    hop(fg_start);
                    fg_end->next = next_after_fg;
                    current_cmd = next_after_fg;
                    continue;
                }

                child_pid = fork();
                if (child_pid == 0) {
                    setpgid(0, 0);
                    signal(SIGINT, SIG_DFL);
                    signal(SIGTSTP, SIG_DFL);
                    signal(SIGQUIT, SIG_DFL);
                    signal(SIGTTIN, SIG_DFL);
                    signal(SIGTTOU, SIG_DFL);
                    signal(SIGCHLD, SIG_DFL);
                    execute(fg_start);
                    exit(0);
                } else {
                    setpgid(child_pid, child_pid);
                    pid_t shell_pgid = getpgrp();
                    if (isatty(STDIN_FILENO)) {
                        tcsetpgrp(STDIN_FILENO, child_pid);
                    }
                    int stat;
                    fg_stopped = 0;
                    fg_pid = -1;
                    waitpid(child_pid, &stat, WUNTRACED);
                    if (isatty(STDIN_FILENO)) {
                        tcsetpgrp(STDIN_FILENO, shell_pgid);
                    }
                    if (fg_stopped || (WIFSTOPPED(stat))) {
                        if (bg_job_count < MAX_BG_JOBS) {
                            int assigned_job_num = next_job_num++;
                            bg_jobs[bg_job_count].job_num = assigned_job_num;
                            bg_jobs[bg_job_count].pid = child_pid;
                            strncpy(bg_jobs[bg_job_count].cmd_name, fg_cmd_name, 255);
                            bg_jobs[bg_job_count].cmd_name[255] = '\0';
                            memset(bg_jobs[bg_job_count].cmd_line, 0, sizeof(bg_jobs[bg_job_count].cmd_line));
                            strncpy(bg_jobs[bg_job_count].cmd_line, fg_cmd_name, sizeof(bg_jobs[bg_job_count].cmd_line) - 1);
                            bg_jobs[bg_job_count].active = 1;
                            bg_jobs[bg_job_count].stopped = 1;
                            printf("\n[%d] Stopped %s\n", assigned_job_num, fg_cmd_name);
                            bg_job_count++;
                        }
                    }
                    child_pid = -1;
                }
                fg_end->next = next_after_fg;
                current_cmd = next_after_fg;
            }
        }
        }
}