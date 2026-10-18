#ifndef CLI_INCLUDE
#define CLI_INCLUDE

#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>

typedef struct {
    char *data;
    ptrdiff_t length;
} Cli_Text;

#define CLI_TEXT(literal) ((Cli_Text){(literal), sizeof(literal) - 1})

typedef enum {
    CLI_OK,
    CLI_HELP,
    CLI_INVALID_SCHEMA,
    CLI_UNKNOWN_OPTION,
    CLI_UNEXPECTED_POSITIONAL,
    CLI_MISSING_VALUE,
    CLI_MISSING_ARGUMENT,
    CLI_DUPLICATE_ARGUMENT,
    CLI_INVALID_VALUE
} Cli_Status;

typedef struct Cli_Argument Cli_Argument;
typedef struct Cli_Error Cli_Error;

typedef Cli_Status (*Cli_Converter)(Cli_Text text, void *destination, void *context, Cli_Error *error);

typedef enum {
    CLI_VALUE_REQUIRED,
    CLI_NO_VALUE
} Cli_ValueMode;

#define CLI_PRIVATE_OPTION_FIELDS   \
    char short_name;                \
    char *long_name;                \
    char *label;                    \
    char *description;              \
    bool required;                  \
    Cli_ValueMode value_mode;       \
    Cli_Converter convert;

struct Cli_Argument {
    CLI_PRIVATE_OPTION_FIELDS
    void *destination;
};

struct Cli_Error {
    Cli_Argument argument;
    Cli_Text input;
    int argv_index;
    char *message;
};

Cli_Status cli_convert_flag(Cli_Text text, void *destination, void *context, Cli_Error *error);
Cli_Status cli_convert_text(Cli_Text text, void *destination, void *context, Cli_Error *error);
Cli_Status cli_convert_int(Cli_Text text, void *destination, void *context, Cli_Error *error);
Cli_Status cli_convert_unsigned(Cli_Text text, void *destination, void *context, Cli_Error *error);
Cli_Status cli_convert_double(Cli_Text text, void *destination, void *context, Cli_Error *error);

#define CLI_ARG_FLAG(field, ...) \
    CLI_ARG(bool, field, cli_convert_flag, .value_mode = CLI_NO_VALUE, __VA_ARGS__)

#define CLI_ARG_DOUBLE(field, ...) \
    CLI_ARG(double, field, cli_convert_double, __VA_ARGS__)

#define CLI_ARG_TEXT(field, ...) \
    CLI_ARG(Cli_Text, field, cli_convert_text, __VA_ARGS__)

#define CLI_ARG_INT(field, ...) \
    CLI_ARG(int, field, cli_convert_int, __VA_ARGS__)

#define CLI_ARG_UNSIGNED(field, ...) \
    CLI_ARG(unsigned int, field, cli_convert_unsigned, __VA_ARGS__)

Cli_Status cli_internal_parse(Cli_Argument *arguments, bool *seen, size_t count, void *context,
                              int argc, char *argv[], Cli_Error *error);
void cli_internal_print_help(Cli_Argument *arguments, size_t count, FILE *stream, char *program);

#endif

#if defined(CLI_ARGUMENTS) && !defined(CLI_SCHEMA_DEFINED)
#define CLI_SCHEMA_DEFINED

#ifdef CLI_ARG
#error "CLI_ARG must be undefined when generating the schema"
#else

#define CLI_ARG(type, field, converter, ...) type field;
typedef struct {
    CLI_ARGUMENTS
} Cli_Arguments;
#undef CLI_ARG

#define CLI_ARG(type, field, converter, ...) + 1
// +1 for help
enum { CLI_ARGUMENT_COUNT = 1 CLI_ARGUMENTS };
#undef CLI_ARG

static inline void build_arguments(Cli_Argument arguments[CLI_ARGUMENT_COUNT], Cli_Arguments *out) {
    size_t index = 0;

#define CLI_ARG(type, field, converter, ...) { \
    struct { type default_value; CLI_PRIVATE_OPTION_FIELDS } settings = {   \
        .convert = (converter), __VA_ARGS__                                 \
    };                                                                      \
                                                                            \
    if (out != NULL) out->field = settings.default_value;                   \
                                                                            \
    arguments[index++] = (Cli_Argument) {                                   \
        .short_name = settings.short_name, .long_name = settings.long_name, \
        .label = settings.label, .description = settings.description,       \
        .required = settings.required,                                      \
        .value_mode = settings.value_mode, .convert = settings.convert,     \
        .destination = out != NULL ? &out->field : NULL                     \
    };                                                                      \
}

    CLI_ARGUMENTS
#undef CLI_ARG

    arguments[index] = (Cli_Argument){
        .short_name = 'h', .long_name = "help", .description = "Show usage",
        .value_mode = CLI_NO_VALUE, .convert = cli_convert_flag
    };
}

static inline Cli_Status cli_parse(int argc, char *argv[], Cli_Arguments *out, void *context, Cli_Error *error) {
    Cli_Argument arguments[CLI_ARGUMENT_COUNT] = {0};
    bool seen[CLI_ARGUMENT_COUNT] = {0};
    bool help_requested = false;
    *error = (Cli_Error) {0};

    build_arguments(arguments, out);
    arguments[CLI_ARGUMENT_COUNT - 1].destination = &help_requested;

    Cli_Status status = cli_internal_parse(arguments, seen, CLI_ARGUMENT_COUNT, context, argc, argv, error);

    if (help_requested)
        return CLI_HELP;

    if (status != CLI_OK)
        return status;

    for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
        if (arguments[i].required && !seen[i]) {
            error->argument = arguments[i];
            error->message = "required argument is missing";
            
            return CLI_MISSING_ARGUMENT;
        }
    }

    return CLI_OK;
}

static inline void cli_print_help(FILE *stream, char *program) {
    Cli_Argument arguments[CLI_ARGUMENT_COUNT];
    build_arguments(arguments, NULL);
    cli_internal_print_help(arguments, CLI_ARGUMENT_COUNT, stream, program);
}

#endif
#endif
