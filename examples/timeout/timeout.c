#include "timeout.h"

#include <errno.h>
#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

int timeout_run(Timeout_Options *options) {
    struct timespec start;
    if (clock_gettime(CLOCK_MONOTONIC, &start) == -1) {
        perror("timeout: clock_gettime");
        return -1;
    }

    pid_t child = fork();
    if (child == -1) {
        perror("timeout: fork");
        return -1;
    }

    if (child == 0) {
        execvp(options->command[0], options->command);
        int error = errno;
        fprintf(stderr, "timeout: %s: %s\n", options->command[0], strerror(error));
        _exit(-1);
    }

    bool expired = false;
    int status;
    for (;;) {
        pid_t result = waitpid(child, &status, expired ? 0 : WNOHANG);

        if (result == child) break;

        if (result == -1) {
            if (errno == EINTR) continue;

            perror("timeout: waitpid");
            return -1;
        }

        struct timespec now;
        if (clock_gettime(CLOCK_MONOTONIC, &now) == -1) {
            perror("timeout: clock_gettime");
            goto cleanup;
        }

        double elapsed = (double)(now.tv_sec - start.tv_sec) + (double)(now.tv_nsec - start.tv_nsec) / 1e9;
        double remaining = options->seconds - elapsed;

        if (options->seconds > 0 && remaining <= 0) {
            if (kill(child, options->signal) == -1 && errno != ESRCH) {
                perror("timeout: kill");
                goto cleanup;
            }

            expired = true;
            fprintf(stderr, "timeout: time limit exceeded; sent signal %d\n", options->signal);
            continue;
        }

        struct timespec pause = {.tv_nsec = 10000000};
        if (options->seconds > 0 && remaining < 0.01) pause.tv_nsec = (long)(remaining * 1e9);

        if (nanosleep(&pause, NULL) == -1 && errno != EINTR) {
            perror("timeout: nanosleep");
            goto cleanup;
        }
    }

    if (!expired && WIFEXITED(status)) return WEXITSTATUS(status);

    return -1;

cleanup:
    if (kill(child, SIGKILL) == -1 && errno != ESRCH)
        perror("timeout: cleanup kill");

    while (waitpid(child, &status, 0) == -1) {
        if (errno == EINTR) continue;

        perror("timeout: cleanup waitpid");
        break;
    }

    return -1;
}
