#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

void handler(int sig, siginfo_t *info, void *ucontext) {
    int const errno_save = errno;

    signal(SIGUSR1, SIG_IGN);

    ssize_t written = 0;
    char const str[] = "Set the SIGUSR1 signal handling method to ignore., press Ctrl+D continue.\n";
    size_t len = sizeof(str);
    ssize_t result;

    do {
        result = write(STDOUT_FILENO, str + written, len - written);
        written += result;
    } while (result > 0);
    if (result == -1) {
        written = 0;
        const char *errnodesc = strerrordesc_np(errno);
        size_t err_len = strlen(errnodesc) + 1;
        do {
            result = write(STDERR_FILENO, errnodesc + written, err_len - written);
            written += result;
        } while (result > 0);
    }

    char buf[256];

    while(read(STDIN_FILENO, buf, sizeof(buf)) > 0);

    errno = errno_save;
}

int main() {
    struct sigaction act;
    struct sigaction oldact;
    act.sa_sigaction = &handler;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGUSR1, &act, &oldact) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("Install a signal handle for SIGUSR1.\n");
    pause();

    printf("press Ctrl+C to quit\n");
    pause();
}
