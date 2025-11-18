#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/time.h>

static volatile size_t count = 0;
void handler_alarm(int sig, siginfo_t *info, void *ucontext) {
    if (info->si_code > 0)
        count += 1;
}
const size_t get_count() {
    return count;
}

volatile sig_atomic_t loop = 1;
void handler_usr1(int sig) {
    loop = 0;
}

int main() {
    struct sigaction act;
    act.sa_sigaction = &handler_alarm;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    act.sa_flags = SA_SIGINFO;
    if (sigaction(SIGALRM, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    act.sa_flags = 0;
    act.sa_handler = &handler_usr1;
    if (sigaction(SIGUSR1, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    alarm(3U);
    pause();

    struct itimerval cutitimer;

    printf("Timer expired %zu times.\n", get_count());
    if (getitimer(ITIMER_REAL, &cutitimer) == -1) {
        perror("getitimer");
        exit(EXIT_FAILURE);
    }
    printf("Time interval: %ld.%06lds, remaining time: %ld.%06lds.\n",
           cutitimer.it_interval.tv_sec, cutitimer.it_interval.tv_usec,
           cutitimer.it_value.tv_sec, cutitimer.it_value.tv_usec);

    struct itimerval timer = {
        .it_interval={ .tv_sec=5, .tv_usec=0, },
        .it_value={ .tv_sec=1, .tv_usec=0, },
    };
    if (setitimer(ITIMER_REAL, &timer, NULL) == -1) {
        perror("setitimer");
        exit(EXIT_FAILURE);
    }

    while (loop) {
        pause();
        printf("Timer expired %lu times.\n", get_count());
        if (getitimer(ITIMER_REAL, &cutitimer) == -1) {
            perror("getitimer");
            exit(EXIT_FAILURE);
        }
        printf("Time interval: %ld.%06lds, remaining time: %ld.%06lds.\n",
               cutitimer.it_interval.tv_sec, cutitimer.it_interval.tv_usec,
               cutitimer.it_value.tv_sec, cutitimer.it_value.tv_usec);
    }

    return 0;
}
