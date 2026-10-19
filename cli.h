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

typedef struct Cli_ParseError Cli_ParseError;
typedef struct Cli_ParsedArgs Cli_ParsedArgs;

typedef Cli_ParseStatus (*Cli_Parser)(char *input, void *destination, void *context, Cli_ParseError *error);

typedef enum {
    CLI_VALUE_REQUIRED,
    CLI_NO_VALUE
} Cli_ValueMode;

struct Cli_ParseError {
    char *input;
    char *message;
};

Cli_ParseStatus cli_parse(int argc, char *argv[], Cli_ParsedArgs *out, void *context, Cli_ParseError *error);
void cli_print_usage(FILE *stream, char *program);
void cli_print_help(FILE *stream);

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

#endif

#if defined(CLI_ARGUMENTS) && !defined(CLI_SCHEMA_DEFINED)
#define CLI_SCHEMA_DEFINED

#ifdef CLI_ARG
#error "CLI_ARG must be undefined when generating the schema"
#else

#define CLI_ARG(type, field, ...) type field;
struct Cli_ParsedArgs {
    CLI_ARGUMENTS
};
#undef CLI_ARG

#define CLI_ARG(...) + 1
// +1 for help
enum { CLI_ARGUMENT_COUNT = 1 CLI_ARGUMENTS };
#undef CLI_ARG

#endif
#endif

#if defined(CLI_IMPLEMENTATION) && defined(CLI_ARGUMENTS) && !defined(CLI_IMPLEMENTATION_INCLUDED)
#define CLI_IMPLEMENTATION_INCLUDED

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#define CLI_PRIVATE_OPTION_FIELDS   \
    char short_name;                \
    char *long_name;                \
    char *label;                    \
    char *description;              \
    bool required;                  \
    Cli_ValueMode value_mode;       \
    Cli_Parser parse;

typedef struct {
    CLI_PRIVATE_OPTION_FIELDS
    void *destination;
} ArgumentSpec;

#define CLI_BOLD(text) "\033[1m" text "\033[0m"
#define CLI_UNDERLINE(text) "\033[4m" text "\033[0m"

static Cli_ParseStatus cli_internal_set_error(Cli_ParseError *error, char *input, char *message) {
    *error = (Cli_ParseError) {.input = input, .message = message};
    return CLI_ERROR;
}

Cli_ParseStatus cli_parse_flag(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)input; (void)context; (void)error;
    *(bool *)destination = true;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_text(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context; (void)error;
    *(char **)destination = input;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_i64(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    intmax_t number = strtoimax(input, &end, 10);
    if (end == input || *end != '\0' || errno == ERANGE || number < INT64_MIN || number > INT64_MAX)
        return cli_internal_set_error(error, input, "invalid or out-of-range integer");

    *(int64_t *)destination = (int64_t)number;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_u64(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    uintmax_t number = strtoumax(input, &end, 10);
    if (strchr(input, '-') || end == input || *end != '\0' || errno == ERANGE || number > UINT64_MAX)
        return cli_internal_set_error(error, input, "invalid or out-of-range unsigned integer");

    *(uint64_t *)destination = (uint64_t)number;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_double(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    double number = strtod(input, &end);
    if (end == input || *end != '\0' || errno == ERANGE || !isfinite(number))
        return cli_internal_set_error(error, input, "invalid or out-of-range floating-point number");

    *(double *)destination = number;
    return CLI_OK;
}

static Cli_ParseStatus cli_internal_parse(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], bool seen[CLI_ARGUMENT_COUNT],
                                          void *context, int argc, char *argv[], Cli_ParseError *error);
static void cli_internal_build_argument_spec(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], Cli_ParsedArgs *out);
static void cli_internal_print_usage(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], FILE *stream, char *program);
static void cli_internal_print_help(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], FILE *stream);

Cli_ParseStatus cli_parse(int argc, char *argv[], Cli_ParsedArgs *out, void *context, Cli_ParseError *error) {
    ArgumentSpec arguments[CLI_ARGUMENT_COUNT] = {0};
    bool seen[CLI_ARGUMENT_COUNT] = {0};
    bool help_requested = false;
    *error = (Cli_ParseError) {0};

    cli_internal_build_argument_spec(arguments, out);
    arguments[CLI_ARGUMENT_COUNT - 1].destination = &help_requested;

    Cli_ParseStatus status = cli_internal_parse(arguments, seen, context, argc, argv, error);

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

#ifndef NDEBUG
static Cli_ParseStatus 
cli_internal_validate_argument_spec(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], Cli_ParseError *error) {
    bool optional_positional = false;
    for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
        ArgumentSpec *argument = &arguments[i];

        char *message = NULL;
        if (!argument->parse) message = "argument needs a parser";

        else if (argument->value_mode != CLI_VALUE_REQUIRED && argument->value_mode != CLI_NO_VALUE)
            message = "invalid value mode";

        else if (argument->short_name 
                 && (argument->short_name <= ' ' || argument->short_name > '~' ||
                     argument->short_name == '-' || argument->short_name == '='))
            message = "invalid short option name";

        if (argument->long_name) {
            if (!argument->long_name[0] || argument->long_name[0] == '-') 
                message = "invalid long option name";

            for (char *p = argument->long_name; *p; ++p)
                if (*p == '=' || isspace((unsigned char)*p)) message = "invalid long option name";
        }

        if (!argument->short_name && !argument->long_name) {
            if (!argument->label) message = "positional argument must have a label";
            if (argument->value_mode == CLI_NO_VALUE) message = "positional argument must take a value";
            if (argument->required && optional_positional) message = "required positional argument follows an optional one";
            if (!argument->required) optional_positional = true;
        }

        for (size_t j = 0; j < i; ++j) {
            if ((argument->short_name && argument->short_name == arguments[j].short_name)
                || (argument->long_name && arguments[j].long_name
                    && strcmp(argument->long_name, arguments[j].long_name) == 0))
                message = "duplicate option name";
        }

        if (message != NULL)
            return cli_internal_set_error(error, argument->long_name ? argument->long_name : argument->label, message);
    }
    return CLI_OK;
}
#endif //NDEBUG

static Cli_ParseStatus cli_internal_parse(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], bool seen[CLI_ARGUMENT_COUNT],
                                          void *context, int argc, char *argv[], Cli_ParseError *error) {
#ifndef NDEBUG
    Cli_ParseStatus status = cli_internal_validate_argument_spec(arguments, error);
    if (status != CLI_OK) return status;
#endif

    for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) seen[i] = false;

    bool options = true;
    size_t positional = 0;
    for (int i = 1; i < argc; ++i) {
        char *token = argv[i];
        if (options && strcmp(token, "--") == 0) {
            options = false;
            continue;
        }
        bool named = options && token[0] == '-' && token[1] != '\0';
        bool long_option = named && token[1] == '-';
        char *short_option = named && !long_option ? token + 1 : NULL;
        char *equals = long_option ? strchr(token + 2, '=') : NULL;
        size_t name_length = long_option ? (equals ? (size_t)(equals - token - 2) : strlen(token + 2)) : 0;

        do {
            size_t match = CLI_ARGUMENT_COUNT;
            if (named) {
                for (size_t j = 0; j < CLI_ARGUMENT_COUNT; ++j) {
                    if (long_option) {
                        char *name = arguments[j].long_name;
                        if (name && strlen(name) == name_length && memcmp(name, token + 2, name_length) == 0)
                            match = j;
                    } else if (arguments[j].short_name == *short_option) {
                        match = j;
                    }
                }
                if (short_option != NULL) ++short_option;
            } else {
                while (positional < CLI_ARGUMENT_COUNT && (arguments[positional].short_name || arguments[positional].long_name))
                    ++positional;
                if (positional < CLI_ARGUMENT_COUNT) match = positional++;
            }
            if (match == CLI_ARGUMENT_COUNT)
                return cli_internal_set_error(error, token,
                                 named ? "unknown option" : "unexpected positional argument");

            ArgumentSpec *argument = &arguments[match];
            if (seen[match])
                return cli_internal_set_error(error, token, "argument was already supplied");
            char *value = NULL;
            if (argument->value_mode == CLI_VALUE_REQUIRED) {
                if (!named) value = token;
                else if (equals != NULL) value = equals + 1;
                else if (short_option != NULL && *short_option != '\0') value = short_option;
                else if (i + 1 < argc) value = argv[++i];
                else return cli_internal_set_error(error, token, "option requires a value");
                short_option = NULL;
            } else if (equals != NULL) {
                return cli_internal_set_error(error, token, "flag does not take a value");
            }

            *error = (Cli_ParseError){0};
            Cli_ParseStatus status = argument->parse(value, argument->destination, context, error);
            if (status != CLI_OK) {
                if (error->input == NULL) error->input = value;
                if (error->message == NULL) error->message = "invalid argument value";
                return status;
            }
            seen[match] = true;
        } while (short_option != NULL && *short_option != '\0');
    }
    *error = (Cli_ParseError){0};
    return CLI_OK;
}

typedef enum {
    ARGUMENT_GROUP_FLAG,
    ARGUMENT_GROUP_OPTION,
    ARGUMENT_GROUP_POSITIONAL,
    ARGUMENT_GROUP_COUNT
} ArgumentGroup;

static ArgumentGroup cli_internal_argument_group(ArgumentSpec *argument) {
    if (!argument->short_name && !argument->long_name) return ARGUMENT_GROUP_POSITIONAL;
    return argument->value_mode == CLI_NO_VALUE ? ARGUMENT_GROUP_FLAG : ARGUMENT_GROUP_OPTION;
}

static void cli_internal_build_argument_spec(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], Cli_ParsedArgs *out) {
    size_t index = 0;

#define CLI_ARG(type, field, parser, ...) { \
    struct { type default_value; CLI_PRIVATE_OPTION_FIELDS } settings = {   \
        .parse = (parser), __VA_ARGS__                                 \
    };                                                                      \
                                                                            \
    if (out != NULL) out->field = settings.default_value;                   \
                                                                            \
    arguments[index++] = (ArgumentSpec) {                               \
        .short_name = settings.short_name, .long_name = settings.long_name, \
        .label = settings.label, .description = settings.description,       \
        .required = settings.required,                                      \
        .value_mode = settings.value_mode, .parse = settings.parse,     \
        .destination = out != NULL ? &out->field : NULL                     \
    };                                                                      \
}

    CLI_ARGUMENTS
#undef CLI_ARG

    arguments[index] = (ArgumentSpec){
        .short_name = 'h', .long_name = "help", .description = "Show usage",
        .value_mode = CLI_NO_VALUE, .parse = cli_parse_flag
    };
}

void cli_print_usage(FILE *stream, char *program) {
    ArgumentSpec arguments[CLI_ARGUMENT_COUNT];
    cli_internal_build_argument_spec(arguments, NULL);
    cli_internal_print_usage(arguments, stream, program);
}

static void cli_internal_print_usage(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], FILE *stream, char *program) {
    fprintf(stream, "Usage: %s", program);

    bool short_flags = false;
    for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
        ArgumentSpec *argument = &arguments[i];
        if (argument->value_mode != CLI_NO_VALUE || !argument->short_name || argument->required) continue;
        if (!short_flags) fputs(" [-", stream);
        fputc(argument->short_name, stream);
        short_flags = true;
    }
    if (short_flags) fputc(']', stream);

    for (ArgumentGroup group = ARGUMENT_GROUP_FLAG; group < ARGUMENT_GROUP_COUNT; ++group) {
        for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
            ArgumentSpec *argument = &arguments[i];

            if (cli_internal_argument_group(argument) != group) continue;
            if (group == ARGUMENT_GROUP_FLAG && argument->short_name && !argument->required) continue;

            fputc(' ', stream);
            if (!argument->required) fputc('[', stream);

            if (argument->short_name) fprintf(stream, "-%c", argument->short_name);
            else if (argument->long_name) fprintf(stream, "--%s", argument->long_name);

            if (argument->value_mode == CLI_VALUE_REQUIRED) {
                char *label = argument->label ? argument->label : "VALUE";
                if (group != ARGUMENT_GROUP_POSITIONAL) fputc(' ', stream);
                fprintf(stream, "%s", label);
            }

            if (!argument->required) fputc(']', stream);
        }
    }

    fputc('\n', stream);
}

void cli_print_help(FILE *stream) {
    ArgumentSpec arguments[CLI_ARGUMENT_COUNT];
    cli_internal_build_argument_spec(arguments, NULL);
    cli_internal_print_help(arguments, stream);
}

static void cli_internal_print_help(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], FILE *stream) {
    fputs("Arguments:\n", stream);

    for (ArgumentGroup group = ARGUMENT_GROUP_FLAG; group < ARGUMENT_GROUP_COUNT; ++group) {
        for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
            ArgumentSpec *argument = &arguments[i];
            if (cli_internal_argument_group(argument) != group) continue;

            char *label = argument->label ? argument->label : "value";
            fputs("  ", stream);

            if (argument->short_name) {
                fprintf(stream, CLI_BOLD("-%c"), argument->short_name);
                if (argument->value_mode == CLI_VALUE_REQUIRED)
                    fprintf(stream, " " CLI_UNDERLINE("%s"), label);
            }

            if (argument->long_name) {
                if (argument->short_name) fputs(", ", stream);
                fprintf(stream, CLI_BOLD("--%s"), argument->long_name);
                if (argument->value_mode == CLI_VALUE_REQUIRED)
                    fprintf(stream, "=" CLI_UNDERLINE("%s"), label);
            }

            if (group == ARGUMENT_GROUP_POSITIONAL) fprintf(stream, CLI_UNDERLINE("%s"), label);

            if (argument->required) fputs(" (required)", stream);
            fputc('\n', stream);

            if (argument->description != NULL) fprintf(stream, "      %s\n", argument->description);
            fputc('\n', stream);
        }
    }
}

#endif
