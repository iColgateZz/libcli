#ifndef CLI_INCLUDE
#define CLI_INCLUDE

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

typedef enum {
    CLI_OK,
    CLI_HELP,
    CLI_ERROR
} Cli_ParseStatus;

typedef struct Cli_ArgumentSpec Cli_ArgumentSpec;
typedef struct Cli_ParseError Cli_ParseError;

typedef Cli_ParseStatus (*Cli_Parser)(char *input, void *destination, void *context, Cli_ParseError *error);

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
    Cli_Parser parse;

struct Cli_ArgumentSpec {
    CLI_PRIVATE_OPTION_FIELDS
    void *destination;
};

struct Cli_ParseError {
    char *input;
    char *message;
};

Cli_ParseStatus cli_parse_flag(char *input, void *destination, void *context, Cli_ParseError *error);
Cli_ParseStatus cli_parse_text(char *input, void *destination, void *context, Cli_ParseError *error);
Cli_ParseStatus cli_parse_i64(char *input, void *destination, void *context, Cli_ParseError *error);
Cli_ParseStatus cli_parse_u64(char *input, void *destination, void *context, Cli_ParseError *error);
Cli_ParseStatus cli_parse_double(char *input, void *destination, void *context, Cli_ParseError *error);

#define CLI_ARG_FLAG(field, ...) \
    CLI_ARG(bool, field, cli_parse_flag, .value_mode = CLI_NO_VALUE, __VA_ARGS__)

#define CLI_ARG_DOUBLE(field, ...) \
    CLI_ARG(double, field, cli_parse_double, __VA_ARGS__)

#define CLI_ARG_TEXT(field, ...) \
    CLI_ARG(char *, field, cli_parse_text, __VA_ARGS__)

#define CLI_ARG_I64(field, ...) \
    CLI_ARG(int64_t, field, cli_parse_i64, __VA_ARGS__)

#define CLI_ARG_U64(field, ...) \
    CLI_ARG(uint64_t, field, cli_parse_u64, __VA_ARGS__)

Cli_ParseStatus cli_internal_parse(Cli_ArgumentSpec *arguments, bool *seen, size_t count, void *context,
                              int argc, char *argv[], Cli_ParseError *error);
void cli_internal_print_help(Cli_ArgumentSpec *arguments, size_t count, FILE *stream, char *program);

#endif

#if defined(CLI_ARGUMENTS) && !defined(CLI_SCHEMA_DEFINED)
#define CLI_SCHEMA_DEFINED

#ifdef CLI_ARG
#error "CLI_ARG must be undefined when generating the schema"
#else

#define CLI_ARG(type, field, parser, ...) type field;
typedef struct {
    CLI_ARGUMENTS
} Cli_ParsedArgs;
#undef CLI_ARG

#define CLI_ARG(type, field, parser, ...) + 1
// +1 for help
enum { CLI_ARGUMENT_COUNT = 1 CLI_ARGUMENTS };
#undef CLI_ARG

static inline void build_argument_spec(Cli_ArgumentSpec arguments[CLI_ARGUMENT_COUNT], Cli_ParsedArgs *out) {
    size_t index = 0;

#define CLI_ARG(type, field, parser, ...) { \
    struct { type default_value; CLI_PRIVATE_OPTION_FIELDS } settings = {   \
        .parse = (parser), __VA_ARGS__                                 \
    };                                                                      \
                                                                            \
    if (out != NULL) out->field = settings.default_value;                   \
                                                                            \
    arguments[index++] = (Cli_ArgumentSpec) {                               \
        .short_name = settings.short_name, .long_name = settings.long_name, \
        .label = settings.label, .description = settings.description,       \
        .required = settings.required,                                      \
        .value_mode = settings.value_mode, .parse = settings.parse,     \
        .destination = out != NULL ? &out->field : NULL                     \
    };                                                                      \
}

    CLI_ARGUMENTS
#undef CLI_ARG

    arguments[index] = (Cli_ArgumentSpec){
        .short_name = 'h', .long_name = "help", .description = "Show usage",
        .value_mode = CLI_NO_VALUE, .parse = cli_parse_flag
    };
}

static inline Cli_ParseStatus cli_parse(int argc, char *argv[], Cli_ParsedArgs *out, void *context, Cli_ParseError *error) {
    Cli_ArgumentSpec arguments[CLI_ARGUMENT_COUNT] = {0};
    bool seen[CLI_ARGUMENT_COUNT] = {0};
    bool help_requested = false;
    *error = (Cli_ParseError) {0};

    build_argument_spec(arguments, out);
    arguments[CLI_ARGUMENT_COUNT - 1].destination = &help_requested;

    Cli_ParseStatus status = cli_internal_parse(arguments, seen, CLI_ARGUMENT_COUNT, context, argc, argv, error);

    if (help_requested)
        return CLI_HELP;

    if (status != CLI_OK)
        return status;

    for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
        if (arguments[i].required && !seen[i]) {
            error->input = arguments[i].long_name ? arguments[i].long_name : arguments[i].label;
            error->message = "required argument is missing";
            
            return CLI_ERROR;
        }
    }

    return CLI_OK;
}

static inline void cli_print_help(FILE *stream, char *program) {
    Cli_ArgumentSpec arguments[CLI_ARGUMENT_COUNT];
    build_argument_spec(arguments, NULL);
    cli_internal_print_help(arguments, CLI_ARGUMENT_COUNT, stream, program);
}

#endif
#endif
