#include "list.h"
#include "timeout.h"

#define CLI_IMPLEMENTATION
#include "../../cli.h"

#include <limits.h>
#include <signal.h>

LIST_DEFINE(char *, Timeout_Arguments);

enum { COMMAND_CAPACITY = 256 };
static char *command_items[COMMAND_CAPACITY + 1];

static Cli_ParseStatus parse_signal(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    uint64_t number;
    struct sigaction action;

    if (cli_parse_u64(input, &number, NULL, error) == CLI_OK && number > 0 && number <= INT_MAX &&
        (number == SIGKILL || number == SIGSTOP || sigaction((int)number, NULL, &action) == 0)) {
        *(int *)destination = (int)number;
        return CLI_OK;
    }

    *error = (Cli_ParseError){.input = input, .message = "expected a valid positive signal number"};
    return CLI_ERROR;
}

static Cli_ParseStatus parse_seconds(char *input, void *destination, void *context, Cli_ParseError *error) {
    double seconds;

    if (cli_parse_double(input, &seconds, context, error) != CLI_OK)
        return CLI_ERROR;

    if (seconds < 0) {
        *error = (Cli_ParseError){.input = input, .message = "seconds must not be negative"};
        return CLI_ERROR;
    }

    *(double *)destination = seconds;
    return CLI_OK;
}

static Cli_ParseStatus parse_argument(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    Timeout_Arguments *arguments = destination;

    if (list_append(arguments, input))
        return CLI_OK;

    *error = (Cli_ParseError){.input = input, .message = "too many command arguments (maximum 255 after COMMAND)"};
    return CLI_ERROR;
}

#define CLI_ARGUMENTS                                                                                                  \
    CLI_ARG(int, signal, parse_signal, .short_name = 's', .label = "SIG", .default_value = SIGTERM,                    \
            .description = "Signal number sent on timeout (default 15, SIGTERM)")                                      \
    CLI_ARG_POSITIONAL(double, seconds, parse_seconds, .required = true, .label = "SECS",                              \
                       .description = "Time limit in seconds; fractions allowed, 0 disables the limit")                \
    CLI_ARG_POSITIONAL_LIST(Timeout_Arguments, command, parse_argument, .required = true, .label = "COMMAND",          \
                            .default_value = {.items = command_items, .capacity = COMMAND_CAPACITY},                   \
                            .description = "Command followed by optional arguments, passed unchanged")

#include "../../cli.h"
#undef CLI_ARGUMENTS

int main(int argc, char *argv[]) {
    Cli_ParsedArgs args;
    Cli_ParseError error;
    Cli_ParseStatus status = cli_parse(argc, argv, &args, NULL, &error);

    if (status == CLI_HELP) {
        cli_print_usage(stdout, argv[0]);
        fputc('\n', stdout);
        cli_print_help(stdout);
        return 0;
    }

    if (status != CLI_OK) {
        fprintf(stderr, "timeout: %s", error.message);
        if (error.input) fprintf(stderr, ": '%s'", error.input);
        fputc('\n', stderr);
        cli_print_usage(stderr, argv[0]);
        return -1;
    }

    args.command.items[args.command.size] = NULL;
    Timeout_Options options = {
        .seconds = args.seconds,
        .signal = args.signal,
        .command = args.command.items
    };

    return timeout_run(&options);
}
