#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>

void handler(int sig, siginfo_t *info, void *ucontext) {
   int const errno_save = errno;

    ssize_t written = 0;
    char const str[] = "enter handler, press Ctrl+D continue.\n";
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
    result = 0;

    while(read(STDIN_FILENO, buf, sizeof(buf)) > 0);

    errno = errno_save;
}

int main() {
    printf("Before installing a signal handle, press enter to continue...> ");
    while(getchar() != '\n');

    struct sigaction act;
    struct sigaction oldact;
    act.sa_sigaction = &handler;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGALRM, &act, &oldact) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("Install a signal handle for SIGALRM.\n");
    pause();

    if (sigaction(SIGALRM, &oldact, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("Install a default signal handle for SIGALRM.\n");

    printf("press enter to quit...> ");
    while(getchar() != '\n');
}
