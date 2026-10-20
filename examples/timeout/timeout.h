#ifndef TIMEOUT_H
#define TIMEOUT_H

typedef struct {
    double seconds;
    int signal;
    char **command;
} Timeout_Options;

int timeout_run(Timeout_Options *options);

#endif
