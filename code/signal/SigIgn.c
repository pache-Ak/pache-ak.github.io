#include <stdio.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>

int main() {
    struct sigaction act = { 0 };

    act.sa_handler = SIG_IGN;
    if (sigemptyset(&act.sa_mask) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    act.sa_flags = 0;
    if (sigaction(SIGSEGV, &act, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("Ignore signal SIGSEGV and pause.\n");
    printf("You can try sending the SIGSENV signal to see if the process is awakened.\n");
    printf("Enter Ctrl+C to quit process.\n");
    pause();
}
