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

//TODO: better names and out-of-the-box versions
#define CLI_ARG(type, field, parser, ...) \
    CLI_PRIVATE_ARG(OPTION, type, field, parser, __VA_ARGS__)

#define CLI_ARG_FLAG(field, ...) \
    CLI_PRIVATE_ARG(FLAG, bool, field, cli_parse_flag, __VA_ARGS__)

#define CLI_ARG_LIST(type, field, parser, ...) \
    CLI_PRIVATE_ARG(OPTION_LIST, type, field, parser, __VA_ARGS__)

#define CLI_ARG_POSITIONAL(type, field, parser, ...) \
    CLI_PRIVATE_ARG(POSITIONAL, type, field, parser, __VA_ARGS__)

#define CLI_ARG_POSITIONAL_LIST(type, field, parser, ...) \
    CLI_PRIVATE_ARG(POSITIONAL_LIST, type, field, parser, __VA_ARGS__)

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

#ifdef CLI_PRIVATE_ARG
#error "CLI_PRIVATE_ARG is reserved for schema expansion"
#else

#define CLI_PRIVATE_ARG(kind, type, field, ...) type field;
struct Cli_ParsedArgs {
    CLI_ARGUMENTS
};
#undef CLI_PRIVATE_ARG

#define CLI_PRIVATE_ARG(...) + 1
// +1 for help
enum { CLI_ARGUMENT_COUNT = 1 CLI_ARGUMENTS };
#undef CLI_PRIVATE_ARG

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

typedef enum {
    ARGUMENT_FLAG,
    ARGUMENT_OPTION,
    ARGUMENT_OPTION_LIST,
    ARGUMENT_POSITIONAL,
    ARGUMENT_POSITIONAL_LIST,
    ARGUMENT_KIND_COUNT
} ArgumentKind;

typedef struct {
    ArgumentKind kind;
    char short_name;
    char *long_name;
    char *label;
    char *description;
    bool required;
    Cli_Parser parse;
    void *destination;
} ArgumentSpec;

#define CLI_BOLD(text) "\033[1m" text "\033[0m"
#define CLI_UNDERLINE(text) "\033[4m" text "\033[0m"

static inline Cli_ParseStatus cli__set_error(Cli_ParseError *error, char *input, char *message) {
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
        return cli__set_error(error, input, "invalid or out-of-range integer");

    *(int64_t *)destination = (int64_t)number;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_u64(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    uintmax_t number = strtoumax(input, &end, 10);
    if (strchr(input, '-') || end == input || *end != '\0' || errno == ERANGE || number > UINT64_MAX)
        return cli__set_error(error, input, "invalid or out-of-range unsigned integer");

    *(uint64_t *)destination = (uint64_t)number;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_double(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    double number = strtod(input, &end);
    if (end == input || *end != '\0' || errno == ERANGE || !isfinite(number))
        return cli__set_error(error, input, "invalid or out-of-range floating-point number");

    *(double *)destination = number;
    return CLI_OK;
}

static bool cli__is_positional(ArgumentSpec *argument) {
    return argument->kind == ARGUMENT_POSITIONAL || argument->kind == ARGUMENT_POSITIONAL_LIST;
}

static Cli_ParseStatus cli__parse(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], bool seen[CLI_ARGUMENT_COUNT],
                                          void *context, int argc, char *argv[], Cli_ParseError *error);
static void cli__build_argument_spec(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], Cli_ParsedArgs *out);
static void cli__print_usage(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], FILE *stream, char *program);
static void cli__print_help(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], FILE *stream);
#ifndef NDEBUG
static Cli_ParseStatus cli__validate_argument_spec(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], Cli_ParseError *error);
#endif

Cli_ParseStatus cli_parse(int argc, char *argv[], Cli_ParsedArgs *out, void *context, Cli_ParseError *error) {
    ArgumentSpec arguments[CLI_ARGUMENT_COUNT] = {0};
    bool seen[CLI_ARGUMENT_COUNT] = {0};
    bool help_requested = false;
    *error = (Cli_ParseError) {0};

    cli__build_argument_spec(arguments, out);
    arguments[CLI_ARGUMENT_COUNT - 1].destination = &help_requested;

    Cli_ParseStatus status = cli__parse(arguments, seen, context, argc, argv, error);

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

static Cli_ParseStatus cli__parse(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], bool seen[CLI_ARGUMENT_COUNT],
                                          void *context, int argc, char *argv[], Cli_ParseError *error) {
#ifndef NDEBUG
    Cli_ParseStatus status = cli__validate_argument_spec(arguments, error);
    if (status != CLI_OK) return status;
#endif

    for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) seen[i] = false;

    bool allow_options = true;
    size_t positional = 0;

    for (int i = 1; i < argc; ++i) {
        char *token = argv[i];
        if (allow_options && strcmp(token, "--") == 0) {
            allow_options = false;
            continue;
        }

        bool named = allow_options && token[0] == '-' && token[1] != '\0';
        bool long_option = named && token[1] == '-';

        char *short_cursor = (named && !long_option) ? token + 1 : NULL;
        char *equals = long_option ? strchr(token + 2, '=') : NULL;

        do {
            size_t match = CLI_ARGUMENT_COUNT;

            if (!named) {
                while (positional < CLI_ARGUMENT_COUNT && !cli__is_positional(&arguments[positional]))
                    ++positional;

                if (positional < CLI_ARGUMENT_COUNT) {
                    match = positional;
                    // Skip over "regular" positional args, stay on list
                    if (arguments[positional].kind == ARGUMENT_POSITIONAL) ++positional;
                }
            }
            else if (long_option) {
                char *option_name = token + 2;
                size_t name_length = equals ? (size_t)(equals - option_name) : strlen(option_name);

                for (size_t j = 0; j < CLI_ARGUMENT_COUNT; ++j) {
                    char *name = arguments[j].long_name;
                    if (name && strlen(name) == name_length && memcmp(name, option_name, name_length) == 0) {
                        match = j;
                        break;
                    }
                }
            }
            else {
                char option_name = *short_cursor;
                for (size_t j = 0; j < CLI_ARGUMENT_COUNT; ++j) {
                    if (arguments[j].short_name == option_name) {
                        match = j;
                        break;
                    }
                }

                // Advance when multple flags are behind one dash. E.g. -xyz.
                ++short_cursor;
            }

            // No candidate found
            if (match == CLI_ARGUMENT_COUNT) {
                char *message = named ? "unknown option" : "unexpected positional argument";
                return cli__set_error(error, token, message);
            }

            if (seen[match] && arguments[match].kind != ARGUMENT_OPTION_LIST
                 && arguments[match].kind != ARGUMENT_POSITIONAL_LIST)
                return cli__set_error(error, token, "argument is duplicated");

            ArgumentSpec *argument = &arguments[match];
            char *value = NULL;
            if (argument->kind != ARGUMENT_FLAG) {
                if (!named) 
                    value = token;
                else if (equals != NULL) 
                    value = equals + 1;
                else if (short_cursor != NULL && *short_cursor != '\0')
                    // Take value directly attached to a short option
                    value = short_cursor;
                else if (i + 1 < argc)
                    value = argv[++i];
                else
                    return cli__set_error(error, token, "option requires a value");

                // Short option is consumed fully or it is NULL from the start
                short_cursor = NULL;
            } 
            else if (equals != NULL) {
                return cli__set_error(error, token, "flag does not take a value");
            }

            *error = (Cli_ParseError){0};
            Cli_ParseStatus status = argument->parse(value, argument->destination, context, error);
            if (status != CLI_OK) {
                if (error->input == NULL) error->input = value;
                if (error->message == NULL) error->message = "invalid argument value";
                return status;
            }

            seen[match] = true;
        } while (short_cursor != NULL && *short_cursor != '\0');
    }

    return CLI_OK;
}

#ifndef NDEBUG
static Cli_ParseStatus 
cli__validate_argument_spec(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], Cli_ParseError *error) {
    bool optional_positional = false;
    bool positional_list = false;

    for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
        ArgumentSpec *argument = &arguments[i];

        char *message = NULL;
        if (!argument->parse) message = "argument needs a parser";

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

        if (cli__is_positional(argument)) {
            if (!argument->label) message = "positional argument must have a label";
            if (positional_list) message = "a positional list must be the last positional argument";
            if (argument->kind == ARGUMENT_POSITIONAL_LIST) {
                positional_list = true;
            } 
            else {
                if (argument->required && optional_positional)
                    message = "required positional argument follows an optional one";
                if (!argument->required) optional_positional = true;
            }
        } 
        else if (!argument->short_name && !argument->long_name) {
            message = "named argument must have a short or long name";
        }

        for (size_t j = 0; j < i; ++j) {
            if ((argument->short_name && argument->short_name == arguments[j].short_name)
                || (argument->long_name && arguments[j].long_name
                    && strcmp(argument->long_name, arguments[j].long_name) == 0))
                message = "duplicate option name";
        }

        if (message != NULL) {
            char *input = argument->long_name ? argument->long_name : argument->label;
            return cli__set_error(error, input, message);
        }
    }

    return CLI_OK;
}
#endif //NDEBUG

#define CLI_PRIVATE_FLAG_FIELDS \
    char short_name; char *long_name; char *description; bool required;

#define CLI_PRIVATE_OPTION_FIELDS \
    CLI_PRIVATE_FLAG_FIELDS char *label;

#define CLI_PRIVATE_OPTION_LIST_FIELDS CLI_PRIVATE_OPTION_FIELDS

#define CLI_PRIVATE_POSITIONAL_FIELDS \
    char *label; char *description; bool required;

#define CLI_PRIVATE_POSITIONAL_LIST_FIELDS CLI_PRIVATE_POSITIONAL_FIELDS

#define CLI_PRIVATE_FLAG_SETTINGS(settings) \
    .short_name = settings.short_name, .long_name = settings.long_name, \
    .description = settings.description, .required = settings.required

#define CLI_PRIVATE_OPTION_SETTINGS(settings) \
    CLI_PRIVATE_FLAG_SETTINGS(settings), .label = settings.label

#define CLI_PRIVATE_OPTION_LIST_SETTINGS(settings) CLI_PRIVATE_OPTION_SETTINGS(settings)

#define CLI_PRIVATE_POSITIONAL_SETTINGS(settings) \
    .label = settings.label, .description = settings.description, .required = settings.required

#define CLI_PRIVATE_POSITIONAL_LIST_SETTINGS(settings) CLI_PRIVATE_POSITIONAL_SETTINGS(settings)

static void cli__build_argument_spec(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], Cli_ParsedArgs *out) {
    size_t index = 0;

#define CLI_PRIVATE_ARG(category, type, field, parser, ...) { \
    struct { type default_value; CLI_PRIVATE_##category##_FIELDS } settings = {__VA_ARGS__}; \
    if (out != NULL) out->field = settings.default_value; \
    arguments[index++] = (ArgumentSpec){ \
        .kind = ARGUMENT_##category, .parse = (parser), \
        CLI_PRIVATE_##category##_SETTINGS(settings), \
        .destination = out != NULL ? &out->field : NULL \
    }; \
}

    CLI_ARGUMENTS
#undef CLI_PRIVATE_ARG

    arguments[index] = (ArgumentSpec){
        .kind = ARGUMENT_FLAG, .short_name = 'h', .long_name = "help", .description = "Show usage",
        .parse = cli_parse_flag
    };
}

#undef CLI_PRIVATE_FLAG_FIELDS
#undef CLI_PRIVATE_OPTION_FIELDS
#undef CLI_PRIVATE_OPTION_LIST_FIELDS
#undef CLI_PRIVATE_POSITIONAL_FIELDS
#undef CLI_PRIVATE_POSITIONAL_LIST_FIELDS
#undef CLI_PRIVATE_FLAG_SETTINGS
#undef CLI_PRIVATE_OPTION_SETTINGS
#undef CLI_PRIVATE_OPTION_LIST_SETTINGS
#undef CLI_PRIVATE_POSITIONAL_SETTINGS
#undef CLI_PRIVATE_POSITIONAL_LIST_SETTINGS

void cli_print_usage(FILE *stream, char *program) {
    ArgumentSpec arguments[CLI_ARGUMENT_COUNT];
    cli__build_argument_spec(arguments, NULL);
    cli__print_usage(arguments, stream, program);
}

static void cli__print_usage(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], FILE *stream, char *program) {
    fprintf(stream, "Usage: %s", program);

    bool short_flags = false;
    for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
        ArgumentSpec *argument = &arguments[i];
        if (argument->kind != ARGUMENT_FLAG || !argument->short_name || argument->required) continue;
        if (!short_flags) fputs(" [-", stream);
        fputc(argument->short_name, stream);
        short_flags = true;
    }
    if (short_flags) fputc(']', stream);

    for (ArgumentKind kind = ARGUMENT_FLAG; kind < ARGUMENT_KIND_COUNT; ++kind) {
        for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
            ArgumentSpec *argument = &arguments[i];

            if (argument->kind != kind) continue;
            if (kind == ARGUMENT_FLAG && argument->short_name && !argument->required) continue;

            fputc(' ', stream);
            if (!argument->required) fputc('[', stream);

            if (argument->short_name) fprintf(stream, "-%c", argument->short_name);
            else if (argument->long_name) fprintf(stream, "--%s", argument->long_name);

            if (argument->kind != ARGUMENT_FLAG) {
                char *label = argument->label ? argument->label : "VALUE";
                if (!cli__is_positional(argument)) fputc(' ', stream);
                fprintf(stream, "%s", label);
                if (kind == ARGUMENT_POSITIONAL_LIST) fputs("...", stream);
            }

            if (!argument->required) fputc(']', stream);
        }
    }

    fputc('\n', stream);
}

void cli_print_help(FILE *stream) {
    ArgumentSpec arguments[CLI_ARGUMENT_COUNT];
    cli__build_argument_spec(arguments, NULL);
    cli__print_help(arguments, stream);
}

static void cli__print_help(ArgumentSpec arguments[CLI_ARGUMENT_COUNT], FILE *stream) {
    fputs("Arguments:\n", stream);

    for (ArgumentKind kind = ARGUMENT_FLAG; kind < ARGUMENT_KIND_COUNT; ++kind) {
        for (size_t i = 0; i < CLI_ARGUMENT_COUNT; ++i) {
            ArgumentSpec *argument = &arguments[i];
            if (argument->kind != kind) continue;

            char *label = argument->label ? argument->label : "value";
            fputs("  ", stream);

            if (argument->short_name) {
                fprintf(stream, CLI_BOLD("-%c"), argument->short_name);
                if (argument->kind != ARGUMENT_FLAG)
                    fprintf(stream, " " CLI_UNDERLINE("%s"), label);
            }

            if (argument->long_name) {
                if (argument->short_name) fputs(", ", stream);
                fprintf(stream, CLI_BOLD("--%s"), argument->long_name);
                if (argument->kind != ARGUMENT_FLAG)
                    fprintf(stream, "=" CLI_UNDERLINE("%s"), label);
            }

            if (cli__is_positional(argument)) {
                fprintf(stream, CLI_UNDERLINE("%s"), label);
                if (kind == ARGUMENT_POSITIONAL_LIST) fputs("...", stream);
            }

            if (argument->required) fputs(" (required)", stream);
            if (argument->kind == ARGUMENT_OPTION_LIST) fputs(" (repeatable)", stream);
            fputc('\n', stream);

            if (argument->description != NULL) fprintf(stream, "      %s\n", argument->description);
            fputc('\n', stream);
        }
    }
}

#endif
