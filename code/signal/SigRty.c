#define _GNU_SOURCE
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>

#define DEFINE_SIGNAL_HANDLER(NAME) \
void handler_##NAME(int sig) { \
    int const errno_save = errno; \
\
    ssize_t written = 0; \
    char const str1[] = "enter handler " #NAME ", press Ctrl+D continue.\n"; \
    size_t len1 = sizeof(str1); \
    ssize_t result; \
\
    do { \
        result = write(STDOUT_FILENO, str1 + written, len1 - written); \
        if (result > 0) written += result; \
    } while (result > 0); \
    if (result == -1) { \
        written = 0; \
        const char *errnodesc = strerrordesc_np(errno); \
        size_t err_len = strlen(errnodesc) + 1; \
        do { \
            result = write(STDERR_FILENO, errnodesc + written, err_len - written); \
            if (result > 0) written += result; \
        } while (result > 0); \
    } \
\
    char buf[256]; \
    \
    do { \
        result = read(STDIN_FILENO, buf, sizeof(buf)); \
    } while(result > 0); \
    if (result == -1) { \
        written = 0; \
        const char *errnodesc = strerrordesc_np(errno); \
        size_t err_len = strlen(errnodesc) + 1; \
        do { \
            result = write(STDERR_FILENO, errnodesc + written, err_len - written); \
            if (result > 0) written += result; \
        } while (result > 0); \
    } \
\
    written = 0; \
    char const str2[] = "leave handler " #NAME ".\n"; \
    size_t len2 = sizeof(str2); \
\
    do { \
        result = write(STDOUT_FILENO, str2 + written, len2 - written); \
        if (result > 0) written += result; \
    } while (result > 0); \
    if (result == -1) { \
        written = 0; \
        const char *errnodesc = strerrordesc_np(errno); \
        size_t err_len = strlen(errnodesc) + 1; \
        do { \
            result = write(STDERR_FILENO, errnodesc + written, err_len - written); \
            if (result > 0) written += result; \
        } while (result > 0); \
    } \
\
    errno = errno_save; \
}

// 使用宏定义具体的信号处理函数
DEFINE_SIGNAL_HANDLER(alrm)
DEFINE_SIGNAL_HANDLER(term)

int main() {
    struct sigaction alrm;
    alrm.sa_handler = handler_alrm;
    if (sigemptyset(&alrm.sa_mask) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    alrm.sa_flags = 0;
    if (sigaction(SIGALRM, &alrm, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    struct sigaction term;
    term.sa_handler = handler_term;
    if (sigemptyset(&term.sa_mask) == -1) {
        perror("sigemptyset");
        exit(EXIT_FAILURE);
    }
    term.sa_flags = 0;
    if (sigaction(SIGTERM, &term, NULL) == -1) {
        perror("sigaction");
        exit(EXIT_FAILURE);
    }

    printf("Install signal handle for SIGALRM and SIGTERM.\n");
    pause();
}
