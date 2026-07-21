#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <errno.h>
#include <ctype.h>
#include <string.h>
#include <sys/types.h>
int is_number(const char *s) {
    if (!s || !*s) return 0;
    for (int i = 0; s[i]; ++i) {
        if (!isdigit(s[i])) return 0;
    }
    return 1;
}

void ping_command(const char *pid_str, const char *sig_str) {
    if (!is_number(pid_str) || !is_number(sig_str)) {
        printf("Invalid syntax!\n");
        return;
    }
    pid_t pid = (pid_t)atoi(pid_str);
    int sig = atoi(sig_str);
    int actual_signal = sig % 32;

    if (kill(pid, 0) == -1) {
        if (errno == ESRCH) {
            printf("No such process found\n");
            return;
        }
    }
    if (kill(pid, actual_signal) == 0) {
        printf("Sent signal %d to process with pid %d\n", sig, pid);
    } else {
        if (errno == ESRCH) {
            printf("No such process found\n");
        } else {
            perror("kill");
        }
    }
}