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
void cli_print_usage(FILE *stream, char *program, Cli_ParsedArgs *args);
void cli_print_help(FILE *stream, Cli_ParsedArgs *args);

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

#define CLI_COMMAND CLI_PRIVATE_COMMAND

#endif //CLI_INCLUDE

//Why?
#if defined(CLI_COMMANDS) && !defined(CLI_ARGUMENTS)
#define CLI_ARGUMENTS
#endif

#if defined(CLI_ARGUMENTS) && !defined(CLI_SCHEMA_DEFINED)
#define CLI_SCHEMA_DEFINED

#define CLI_PRIVATE_ARG(kind, type, field, ...) type field;

#ifdef CLI_COMMANDS
#define CLI_PRIVATE_COMMAND(field, name, description, arguments) \
    typedef struct { unsigned char cli_reserved; arguments } Cli_CommandArgs_##field;
CLI_COMMANDS
#undef CLI_PRIVATE_COMMAND

#define CLI_PRIVATE_COMMAND(field, ...) cli_command_##field,
typedef enum {
    cli_command_none,
    CLI_COMMANDS
} Cli_Command;
#undef CLI_PRIVATE_COMMAND
#endif

// Why?
struct Cli_ParsedArgs {
    unsigned char cli_reserved;
    CLI_ARGUMENTS
#ifdef CLI_COMMANDS
    Cli_Command command;
    union {
        unsigned char cli_reserved;
#define CLI_PRIVATE_COMMAND(field, ...) Cli_CommandArgs_##field field;
        CLI_COMMANDS
#undef CLI_PRIVATE_COMMAND
    } commands;
#endif
};
#undef CLI_PRIVATE_ARG

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

typedef struct Command Command;

typedef struct {
    size_t argument_count;
    void (*build)(ArgumentSpec *arguments, Cli_ParsedArgs *out);
    Command *commands;
    size_t command_count;
} Schema;

struct Command {
    char *name;
    char *description;
    Schema schema;
};

#define CLI_BOLD(text) "\033[1m" text "\033[0m"
#define CLI_UNDERLINE(text) "\033[4m" text "\033[0m"

static Cli_ParseStatus parse_error_set(Cli_ParseError *error, char *input, char *message) {
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
        return parse_error_set(error, input, "invalid or out-of-range integer");

    *(int64_t *)destination = (int64_t)number;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_u64(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    uintmax_t number = strtoumax(input, &end, 10);
    if (strchr(input, '-') || end == input || *end != '\0' || errno == ERANGE || number > UINT64_MAX)
        return parse_error_set(error, input, "invalid or out-of-range unsigned integer");

    *(uint64_t *)destination = (uint64_t)number;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_double(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    double number = strtod(input, &end);
    if (end == input || *end != '\0' || errno == ERANGE || !isfinite(number))
        return parse_error_set(error, input, "invalid or out-of-range floating-point number");

    *(double *)destination = number;
    return CLI_OK;
}

static bool argument_spec_is_positional(ArgumentSpec *argument) {
    return argument->kind == ARGUMENT_POSITIONAL || argument->kind == ARGUMENT_POSITIONAL_LIST;
}

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

#define CLI_PRIVATE_ARG(category, type, field, parser, ...) { \
    struct { type default_value; CLI_PRIVATE_##category##_FIELDS } cli__settings = {__VA_ARGS__}; \
    if (cli__out != NULL) cli__out->field = cli__settings.default_value; \
    cli__arguments[cli__index++] = (ArgumentSpec){ \
        .kind = ARGUMENT_##category, .parse = (parser), \
        CLI_PRIVATE_##category##_SETTINGS(cli__settings), \
        .destination = cli__out != NULL ? &cli__out->field : NULL \
    }; \
}

static void build_root(ArgumentSpec *cli__arguments, Cli_ParsedArgs *cli__out) {
    size_t cli__index = 0;
    CLI_ARGUMENTS
    (void)cli__arguments; (void)cli__out; (void)cli__index;
}

#ifdef CLI_COMMANDS
#define CLI_PRIVATE_COMMAND(field, name, description, schema) \
    static void build_command_##field(ArgumentSpec *cli__arguments, Cli_ParsedArgs *cli__parent) { \
        Cli_CommandArgs_##field *cli__out = cli__parent ? &cli__parent->commands.field : NULL; \
        if (cli__parent) cli__parent->command = cli_command_##field; \
        size_t cli__index = 0; \
        schema \
        (void)cli__arguments; (void)cli__out; (void)cli__index; \
    }
CLI_COMMANDS
#undef CLI_PRIVATE_COMMAND
#endif
#undef CLI_PRIVATE_ARG

#define CLI_PRIVATE_ARG(...) + 1
#ifdef CLI_COMMANDS
#define CLI_PRIVATE_COMMAND(field, command_name, text, arguments) \
    { .name = command_name, .description = text, \
      .schema = {.argument_count = 1 arguments, .build = build_command_##field} },
static Command cli_commands[] = {
    {0},
    CLI_COMMANDS
};
#undef CLI_PRIVATE_COMMAND
#endif

static Schema cli_root_schema = {
    .argument_count = 1 CLI_ARGUMENTS,
    .build = build_root,
#ifdef CLI_COMMANDS
    .commands = cli_commands,
    .command_count = sizeof(cli_commands) / sizeof(cli_commands[0]),
#endif
};
#undef CLI_PRIVATE_ARG

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

static Cli_ParseStatus parse_tokens(size_t count, ArgumentSpec arguments[], bool seen[], void *context,
                                   int argc, char *argv[], int *command_position, Cli_ParseError *error) {
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
            size_t match = count;

            if (!named) {
                if (command_position) {
                    *command_position = i;
                    return CLI_OK;
                }
                while (positional < count && !argument_spec_is_positional(&arguments[positional]))
                    ++positional;

                if (positional < count) {
                    match = positional;
                    // Skip over "regular" positional args, stay on list
                    if (arguments[positional].kind == ARGUMENT_POSITIONAL) ++positional;
                    else allow_options = false;
                }
            }
            else if (long_option) {
                char *option_name = token + 2;
                size_t name_length = equals ? (size_t)(equals - option_name) : strlen(option_name);

                for (size_t j = 0; j < count; ++j) {
                    char *name = arguments[j].long_name;
                    if (name && strlen(name) == name_length && memcmp(name, option_name, name_length) == 0) {
                        match = j;
                        break;
                    }
                }
            }
            else {
                char option_name = *short_cursor;
                for (size_t j = 0; j < count; ++j) {
                    if (arguments[j].short_name == option_name) {
                        match = j;
                        break;
                    }
                }

                // Advance when multple flags are behind one dash. E.g. -xyz.
                ++short_cursor;
            }

            // No candidate found
            if (match == count) {
                char *message = named ? "unknown option" : "unexpected positional argument";
                return parse_error_set(error, token, message);
            }

            if (seen[match] && arguments[match].kind != ARGUMENT_OPTION_LIST
                 && arguments[match].kind != ARGUMENT_POSITIONAL_LIST)
                return parse_error_set(error, token, "argument is duplicated");

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
                    return parse_error_set(error, token, "option requires a value");

                // Short option is consumed fully or it is NULL from the start
                short_cursor = NULL;
            }
            else if (equals != NULL) {
                return parse_error_set(error, token, "flag does not take a value");
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
static Cli_ParseStatus schema_validate(Schema *schema, ArgumentSpec arguments[], Cli_ParseError *error) {
    size_t count = schema->argument_count;
    bool optional_positional = false;
    bool positional_list = false;

    for (size_t i = 0; i < count; ++i) {
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

        if (argument_spec_is_positional(argument)) {
            if (!argument->label) message = "positional argument must have a label";
            if (schema->command_count > 1) message = "commands cannot be combined with positional arguments";
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
            return parse_error_set(error, input, message);
        }
    }

    for (size_t i = 1; i < schema->command_count; ++i) {
        char *name = schema->commands[i].name;
        if (!name || !name[0] || name[0] == '-')
            return parse_error_set(error, name, "invalid command name");
        for (char *p = name; *p; ++p)
            if (isspace((unsigned char)*p)) return parse_error_set(error, name, "invalid command name");
        for (size_t j = 1; j < i; ++j)
            if (strcmp(name, schema->commands[j].name) == 0)
                return parse_error_set(error, name, "duplicate command name");
    }

    return CLI_OK;
}
#endif //NDEBUG

static void schema_build(Schema *schema, ArgumentSpec arguments[], Cli_ParsedArgs *out) {
    schema->build(arguments, out);
    arguments[schema->argument_count - 1] = (ArgumentSpec){
        .kind = ARGUMENT_FLAG, .short_name = 'h', .long_name = "help", .description = "Show usage",
        .parse = cli_parse_flag
    };
}

static Cli_ParseStatus schema_parse(Schema *schema, int argc, char *argv[], Cli_ParsedArgs *out,
                                   void *context, size_t *selected, Cli_ParseError *error) {
    size_t count = schema->argument_count;
    ArgumentSpec arguments[count];
    bool seen[count];
    memset(seen, 0, sizeof(seen));
    bool help_requested = false;
    *selected = 0;
    schema_build(schema, arguments, out);
    arguments[count - 1].destination = &help_requested;
#ifndef NDEBUG
    Cli_ParseStatus valid = schema_validate(schema, arguments, error);
    if (valid != CLI_OK) return valid;
#endif

    int command_position = argc;
    Cli_ParseStatus status = parse_tokens(count, arguments, seen, context, argc, argv,
                                         schema->command_count > 1 ? &command_position : NULL, error);
    if (help_requested) return CLI_HELP;
    if (status != CLI_OK) return status;

    if (schema->command_count > 1) {
        if (command_position == argc) return parse_error_set(error, NULL, "command is required");
        size_t command = 1;
        while (command < schema->command_count &&
               strcmp(argv[command_position], schema->commands[command].name) != 0)
            ++command;
        if (command == schema->command_count)
            return parse_error_set(error, argv[command_position], "unknown command");

        *selected = command;
        size_t child_selected;
        status = schema_parse(&schema->commands[command].schema, argc - command_position,
                              argv + command_position, out, context, &child_selected, error);
        if (status != CLI_OK) return status;
    }

    for (size_t i = 0; i < count; ++i) {
        if (arguments[i].required && !seen[i]) {
            *selected = 0;
            char *input = arguments[i].long_name ? arguments[i].long_name : arguments[i].label;
            return parse_error_set(error, input, "required argument is missing");
        }
    }
    return CLI_OK;
}

Cli_ParseStatus cli_parse(int argc, char *argv[], Cli_ParsedArgs *out, void *context, Cli_ParseError *error) {
    *out = (Cli_ParsedArgs){0};
    *error = (Cli_ParseError){0};
    size_t selected;
    Cli_ParseStatus status = schema_parse(&cli_root_schema, argc, argv, out, context, &selected, error);
#ifdef CLI_COMMANDS
    out->command = (Cli_Command)selected;
#endif
    return status;
}

static Schema *parsed_args_select_schema(Cli_ParsedArgs *args) {
#ifdef CLI_COMMANDS
    if (args && args->command > cli_command_none && (size_t)args->command < cli_root_schema.command_count)
        return &cli_commands[args->command].schema;
#else
    (void)args;
#endif
    return &cli_root_schema;
}

void cli_print_usage(FILE *stream, char *program, Cli_ParsedArgs *args) {
    Schema *schema = parsed_args_select_schema(args);
    size_t count = schema->argument_count;
    ArgumentSpec arguments[count];
    schema_build(schema, arguments, NULL);
    fprintf(stream, "Usage: %s", program);
#ifdef CLI_COMMANDS
    if (schema != &cli_root_schema) fprintf(stream, " %s", cli_commands[args->command].name);
#endif

    bool short_flags = false;
    for (size_t i = 0; i < count; ++i) {
        ArgumentSpec *argument = &arguments[i];
        if (argument->kind != ARGUMENT_FLAG || !argument->short_name || argument->required) continue;
        if (!short_flags) fputs(" [-", stream);
        fputc(argument->short_name, stream);
        short_flags = true;
    }
    if (short_flags) fputc(']', stream);

    for (ArgumentKind kind = ARGUMENT_FLAG; kind < ARGUMENT_KIND_COUNT; ++kind) {
        for (size_t i = 0; i < count; ++i) {
            ArgumentSpec *argument = &arguments[i];

            if (argument->kind != kind) continue;
            if (kind == ARGUMENT_FLAG && argument->short_name && !argument->required) continue;

            fputc(' ', stream);
            if (!argument->required) fputc('[', stream);

            if (argument->short_name) fprintf(stream, "-%c", argument->short_name);
            else if (argument->long_name) fprintf(stream, "--%s", argument->long_name);

            if (argument->kind != ARGUMENT_FLAG) {
                char *label = argument->label ? argument->label : "VALUE";
                if (!argument_spec_is_positional(argument)) fputc(' ', stream);
                fprintf(stream, "%s", label);
                if (kind == ARGUMENT_POSITIONAL_LIST) fputs("...", stream);
            }

            if (!argument->required) fputc(']', stream);
        }
    }

    if (schema->command_count > 1) fputs(" COMMAND [ARGS...]", stream);
    fputc('\n', stream);
}

void cli_print_help(FILE *stream, Cli_ParsedArgs *args) {
    Schema *schema = parsed_args_select_schema(args);
    size_t count = schema->argument_count;
    ArgumentSpec arguments[count];
    schema_build(schema, arguments, NULL);
    fputs("Arguments:\n", stream);

    for (ArgumentKind kind = ARGUMENT_FLAG; kind < ARGUMENT_KIND_COUNT; ++kind) {
        for (size_t i = 0; i < count; ++i) {
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

            if (argument_spec_is_positional(argument)) {
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

    if (schema->command_count > 1) {
        fputs("Commands:\n", stream);
        for (size_t i = 1; i < schema->command_count; ++i) {
            Command *command = &schema->commands[i];
            fprintf(stream, "  " CLI_BOLD("%s") "\n", command->name);
            if (command->description) fprintf(stream, "      %s\n", command->description);
            fputc('\n', stream);
        }
    }
}

#endif
