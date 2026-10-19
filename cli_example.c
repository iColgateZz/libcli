#include "cli.h"

typedef struct {
    uint64_t number;
} Example_Port;

typedef struct {
    uint64_t maximum_port;
} ParseContext;

static Cli_ParseStatus parse_port(char *input, void *destination, void *context, Cli_ParseError *error) {
    ParseContext *limits = context;
    uint64_t number;
    Cli_ParseStatus status = cli_parse_u64(input, &number, NULL, error);

    if (status != CLI_OK)
        return status;

    if (number == 0 || number > limits->maximum_port) {
        error->input = input;
        error->message = "port is outside the allowed range";
        return CLI_ERROR;
    }

    Example_Port *port = destination;
    port->number = number;

    return CLI_OK;
}

#define CLI_ARGUMENTS \
    CLI_ARG_FLAG(verbose, .short_name = 'v', .long_name = "verbose", .description = "Enable verbose output") \
    CLI_ARG_U64(count, .default_value = 4, .short_name = 'c', .long_name = "count", \
        .label = "num", .description = "Request count") \
    CLI_ARG(Example_Port, port, parse_port, .default_value = {80}, \
      .short_name = 'p', .long_name = "port", .label = "port", .description = "Port") \
    CLI_ARG_TEXT(host, .required = true, .label = "host", .description = "Target host") \
    CLI_ARG_TEXT(output, .default_value = "console", .label = "out", .description = "Output destination")

#include "cli.h"


int main(int argc, char **argv) {
    Cli_ParsedArgs args;
    ParseContext context = {.maximum_port = 65535};
    Cli_ParseError error;
    Cli_ParseStatus status = cli_parse(argc, argv, &args, &context, &error);

    if (status == CLI_HELP) {
        cli_print_usage(stderr, argv[0]);
        cli_print_help(stderr, argv[0]);
        return 0;
    }

    if (status != CLI_OK) {
        fprintf(stderr, "argument parsing error");
        if (error.input) fprintf(stderr, " at '%s'", error.input);
        fprintf(stderr, ": %s\n", error.message);

        cli_print_usage(stderr, argv[0]);
        return -1;
    }

    if (args.count == 0) {
        fprintf(stderr, "count must be positive\n");
        return -1;
    }

    if (args.verbose)
        fprintf(stderr, "count=%lld, port=%lld\n", args.count, args.port.number);

    printf("host: %s\noutput: %s\n", args.host, args.output);
    return 0;
}
