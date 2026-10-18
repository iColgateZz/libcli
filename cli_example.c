#include "cli.h"

typedef struct {
    unsigned int number;
} Example_Port;

typedef struct {
    unsigned int maximum_port;
} ParseContext;

static Cli_Status convert_port(Cli_Text text, void *destination, void *context, Cli_Error *error)
{
    ParseContext *limits = context;
    unsigned int number;
    Cli_Status status = cli_convert_unsigned(text, &number, NULL, error);
    if (status != CLI_OK)
        return status;
    if (number == 0 || number > limits->maximum_port) {
        if (error != NULL)
            error->message = "port is outside the allowed range";
        return CLI_INVALID_VALUE;
    }
    Example_Port *port = destination;
    port->number = number;
    return CLI_OK;
}

#define CLI_ARGUMENTS \
    CLI_ARG_FLAG(verbose, \
      .short_name = 'v', .long_name = "verbose", \
      .description = "Enable verbose output") \
    CLI_ARG_UNSIGNED(count, .default_value = 4, \
      .short_name = 'c', .long_name = "count", .label = "NUM", \
      .description = "Request count (default: 4)") \
    CLI_ARG(Example_Port, port, convert_port, .default_value = {80}, \
      .short_name = 'p', .long_name = "port", .label = "PORT", \
      .description = "Port (default: 80)") \
    CLI_ARG_TEXT(host, \
      .required = true, .label = "HOST", .description = "Target host") \
    CLI_ARG_TEXT(output, .default_value = CLI_TEXT("console"), \
      .label = "OUTPUT", .description = "Output destination (default: console)")

#include "cli.h"

static int report_error(Cli_Status status, Cli_Error error)
{
    fprintf(stderr, "argument error %d: %s\n", (int)status,
            error.message != NULL ? error.message : "invalid command line");
    if (error.argv_index > 0)
        fprintf(stderr, "at argv[%d]\n", error.argv_index);
    return 2;
}

int main(int argc, char **argv)
{
    Cli_Arguments values;
    ParseContext context = {.maximum_port = 65535};
    Cli_Error error = {0};
    Cli_Status status = cli_parse(argc, argv, &values, &context, &error);

    if (status == CLI_HELP) {
        cli_print_help(stdout, argv[0]);
        return 0;
    }

    if (status != CLI_OK)
        return report_error(status, error);

    if (values.count == 0) {
        fprintf(stderr, "count must be positive\n");
        return 2;
    }

    if (values.verbose)
        fprintf(stderr, "count=%u, port=%u\n", values.count, values.port.number);

    fputs("host: ", stdout);
    fwrite(values.host.data, 1, values.host.length, stdout);
    fputs("\noutput: ", stdout);
    fwrite(values.output.data, 1, values.output.length, stdout);
    fputc('\n', stdout);
    return ferror(stdout) ? 2 : 0;
}
