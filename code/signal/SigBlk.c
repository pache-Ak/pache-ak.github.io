#define _GNU_SOURCE
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

void handler(int sig, siginfo_t *info, void *ucontext) {}

int main(void) {
    struct sigaction act;
    act.sa_sigaction = &handler;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGALRM, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }


    sigset_t set;
    if (sigemptyset(&set) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    if (sigaddset(&set, SIGALRM) == -1) {
        perror("sigaddset");
        exit(EXIT_FAILURE);
    }
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    printf("SIGALRM block.\n");
    printf("You can try sending the SIGALRM signal and observe the process status.\n");
    printf("Press enter to continue...> ");
    while(getchar() != '\n');
    printf("SIGALRM unblock.\n");
    if (sigprocmask(SIG_BLOCK, &set, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }
    printf("Press enter to quit>");
    while(getchar() != '\n');
}
