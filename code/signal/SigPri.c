#define _GNU_SOURCE
#include <signal.h>
#include <string.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>

ssize_t signal_print(const char *str) {
    ssize_t written = 0;
    size_t len = strlen(str) + 1;
    ssize_t result;

    do {
        result = write(STDOUT_FILENO, str + written, len - written);
        written += result;
    } while (result > 0);
    return result;
}

void signal_perrno(int errum) {
    const char *str = strerrordesc_np(errum);
    ssize_t written = 0;
    size_t len = strlen(str) + 1;
    ssize_t result;

    do {
        result = write(STDERR_FILENO, str + written, len - written);
        written += result;
    } while (result > 0);
}

void handler(int sig, siginfo_t *info, void *ucontext) {
    const int errno_saved = errno;

    sigset_t set;
    sigset_t oldset;
    if (sigfillset(&set) == -1) {
        signal_perrno(errno);
        errno = errno_saved;
        return;
    }
    if (sigprocmask(SIG_SETMASK, &set, &oldset) == -1) {
        signal_perrno(errno);
        goto ERROR_END;
    }

    if (info->si_signo < SIGRTMIN) {
        const char *sigdescr = sigdescr_np(info->si_signo);
        if (sigdescr == NULL) {
            signal_print("invalid signal number!\n");
            goto ERROR_END;
        }
        if (signal_print(sigdescr) == -1) {
            signal_perrno(errno);
        }
        if (signal_print("\n") == -1) {
            signal_perrno(errno);
        }
    } else if (info->si_signo >= SIGRTMIN && info->si_signo <= SIGRTMAX) {
        if (signal_print(info->si_value.sival_ptr) == -1) {
            signal_perrno(errno);
        }
    } else {
        if (signal_print("invalid signal number!\n") == -1) {
            signal_perrno(errno);
        }
    }


ERROR_END:
    if (sigprocmask(SIG_SETMASK, &oldset, NULL) == -1) {
        signal_perrno(errno);
        goto ERROR_END;
    }

    errno = errno_saved;
}

int main() {
    struct sigaction act;
    act.sa_sigaction = &handler;
    // Block all signals to prevent signal reentrancy
    if (sigfillset(&act.sa_mask) == -1) {
        perror("sigfillset");
        exit(EXIT_FAILURE);
    }
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGUSR1, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    if (sigaction(SIGUSR2, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }
    for (int sig = SIGRTMIN; sig != SIGRTMAX + 1; ++sig) {
        if (sigaction(sig, &act, NULL) == -1) {
            perror("sigaction");
            exit(EXIT_FAILURE);
        }
    }

    sigset_t set;
    sigset_t oldset;
    if (sigfillset(&set) == -1) {
        perror("sigfillset");
        exit(EXIT_FAILURE);
    }
    if (sigprocmask(SIG_BLOCK, &set, &oldset) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    printf("All signal block.\n");
    printf("Press enter to send some signal to this process.\n");
    while(getchar() != '\n');

    raise(SIGUSR1);
    raise(SIGUSR2);
    union sigval mes;
    char **messages = malloc(sizeof(char*) * (SIGRTMAX - SIGRTMIN + 1));
    for (int sig = 0; sig <= SIGRTMAX - SIGRTMIN; ++sig) {
        messages[sig] = malloc(48 * sizeof(char));
        snprintf(messages[sig],48, "Real time signal SIGRTMIN + %2d\n", sig);
        mes.sival_ptr = messages[sig];
        sigqueue(getpid(), sig + SIGRTMIN, mes);
        sigqueue(getpid(), sig + SIGRTMIN, mes);
    }

    printf("Press Enter to disable signal blocking.\n");
    while(getchar() != '\n');
    if (sigprocmask(SIG_SETMASK, &oldset, NULL) == -1) {
        perror("sigprocmask");
        exit(EXIT_FAILURE);
    }

    for (int sig = 0; sig <= SIGRTMAX - SIGRTMIN; ++sig) {
        free(messages[sig]);
    }
    free(messages);

    return 0;
}