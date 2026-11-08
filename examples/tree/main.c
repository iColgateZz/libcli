#define CLI_IMPLEMENTATION
#include "schema.h"

int main(int argc, char *argv[]) {
    Cli_Arguments args;
    Cli_ParseError error;
    Cli_ParseStatus status = cli_parse(argc, argv, &args, NULL, &error);

    if (status == CLI_HELP) {
        cli_print_usage(stdout, argv[0], &args);
        cli_print_help(stdout, &args);
        return 0;
    }

    if (status == CLI_ERROR) {
        fprintf(stderr, "%s", error.message);
        if (error.input) fprintf(stderr, ": '%s'", error.input);
        fputc('\n', stderr);
        cli_print_usage(stderr, argv[0], &args);
        return 1;
    }

    if (args.verbose)
        fprintf(stderr, "verbose output enabled\n");

    if (args.cmd_type == Cli_Command_remote) {
        Remote_Arguments remote = args.cmd.remote;

        switch (remote.cmd_type) {
            case Remote_Command_add: {
                Remote_Arguments_add add = remote.cmd.add;
                printf("%s: %s\n", add.name, add.url);
                break;
            }

            case Remote_Command_status: {
                printf("Configuration: %s\n", remote.config);
                break;
            }

            case Remote_Command_none: {
                printf("Unreachable!");
                return 1;
            }
        }
    }

    return 0;
}
