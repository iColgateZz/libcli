#define CLI_IMPLEMENTATION
#include "../../cli.h"

#define CLI_ARGUMENTS \
    CLI_ARG_FLAG(verbose, .short_name = 'v', .long_name = "verbose", .description = "Print extra diagnostics")

#define CLI_COMMANDS \
    CLI_COMMAND(greet, "greet", "Print a greeting", GREET_ARGUMENTS) \
    CLI_COMMAND(add, "add", "Add two numbers", ADD_ARGUMENTS)

#define GREET_ARGUMENTS \
    CLI_ARG_U64(count, .short_name = 'c', .long_name = "count", .default_value = 1, .label = "NUM", \
        .description = "Number of greetings") \
    CLI_ARG_POSITIONAL(char *, name, cli_parse_text, .required = true, .label = "NAME", \
        .description = "Person to greet")

#define ADD_ARGUMENTS \
    CLI_ARG_POSITIONAL(double, left, cli_parse_double, .required = true, .label = "LEFT") \
    CLI_ARG_POSITIONAL(double, right, cli_parse_double, .required = true, .label = "RIGHT")

#include "../../cli.h"

int main(int argc, char *argv[]) {
    Cli_ParsedArgs args;
    Cli_ParseError error;
    Cli_ParseStatus status = cli_parse(argc, argv, &args, NULL, &error);

    if (status == CLI_HELP) {
        cli_print_usage(stderr, argv[0], &args);
        fputc('\n', stderr);
        cli_print_help(stderr, &args);
        return 0;
    }

    if (status == CLI_ERROR) {
        fprintf(stderr, "%s", error.message);
        if (error.input) fprintf(stderr, ": '%s'", error.input);
        fputc('\n', stderr);
        cli_print_usage(stderr, argv[0], &args);
        return 2;
    }

    switch (args.cmd_type) {
        case cli_command_greet:
            if (args.verbose) fprintf(stderr, "command: greet\n");
            for (uint64_t i = 0; i < args.cmd.greet.count; ++i)
                printf("Hello, %s!\n", args.cmd.greet.name);
            break;

        case cli_command_add:
            if (args.verbose) fprintf(stderr, "command: add\n");
            printf("%g\n", args.cmd.add.left + args.cmd.add.right);
            break;

        default: break;
    }

    return 0;
}
