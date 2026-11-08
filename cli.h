#ifndef CLI_INCLUDE
#define CLI_INCLUDE

#define CLI_VERSION_MAJOR 2
#define CLI_VERSION_MINOR 0
#define CLI_VERSION_PATCH 0
#define CLI_VERSION "2.0.0"

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
typedef struct Cli_Arguments Cli_Arguments;

typedef Cli_ParseStatus (*Cli_Parser)(char *input, void *destination, void *context, Cli_ParseError *error);

struct Cli_ParseError {
    char *input;
    char message[256];
};

Cli_ParseStatus cli_parse(int argc, char *argv[], Cli_Arguments *out, void *context, Cli_ParseError *error);
void cli_print_usage(FILE *stream, char *program, Cli_Arguments *args);
void cli_print_help(FILE *stream, Cli_Arguments *args);

Cli_ParseStatus cli_parse_flag(char *input, void *destination, void *context, Cli_ParseError *error);
Cli_ParseStatus cli_parse_text(char *input, void *destination, void *context, Cli_ParseError *error);
Cli_ParseStatus cli_parse_i64(char *input, void *destination, void *context, Cli_ParseError *error);
Cli_ParseStatus cli_parse_u64(char *input, void *destination, void *context, Cli_ParseError *error);
Cli_ParseStatus cli_parse_f64(char *input, void *destination, void *context, Cli_ParseError *error);

#define CLI_ARG_FLAG(field, ...) \
    CLI_PRIVATE_ARG(FLAG, bool, field, cli_parse_flag, __VA_ARGS__)


#define CLI_ARG_OPTION(type, field, parser, ...) \
    CLI_PRIVATE_ARG(OPTION, type, field, parser, __VA_ARGS__)

#define CLI_ARG_OPTION_TEXT(field, ...) \
    CLI_ARG_OPTION(char *, field, cli_parse_text, __VA_ARGS__)

#define CLI_ARG_OPTION_I64(field, ...) \
    CLI_ARG_OPTION(int64_t, field, cli_parse_i64, __VA_ARGS__)

#define CLI_ARG_OPTION_U64(field, ...) \
    CLI_ARG_OPTION(uint64_t, field, cli_parse_u64, __VA_ARGS__)

#define CLI_ARG_OPTION_F64(field, ...) \
    CLI_ARG_OPTION(double, field, cli_parse_f64, __VA_ARGS__)

#define CLI_ARG_OPTION_LIST(type, field, parser, ...) \
    CLI_PRIVATE_ARG(OPTION_LIST, type, field, parser, __VA_ARGS__)


#define CLI_ARG_POSITIONAL(type, field, parser, ...) \
    CLI_PRIVATE_ARG(POSITIONAL, type, field, parser, __VA_ARGS__)

#define CLI_ARG_POSITIONAL_TEXT(field, ...) \
    CLI_ARG_POSITIONAL(char *, field, cli_parse_text, __VA_ARGS__)

#define CLI_ARG_POSITIONAL_I64(field, ...) \
    CLI_ARG_POSITIONAL(int64_t, field, cli_parse_i64, __VA_ARGS__)

#define CLI_ARG_POSITIONAL_U64(field, ...) \
    CLI_ARG_POSITIONAL(uint64_t, field, cli_parse_u64, __VA_ARGS__)

#define CLI_ARG_POSITIONAL_F64(field, ...) \
    CLI_ARG_POSITIONAL(double, field, cli_parse_f64, __VA_ARGS__)

#define CLI_ARG_POSITIONAL_LIST(type, field, parser, ...) \
    CLI_PRIVATE_ARG(POSITIONAL_LIST, type, field, parser, __VA_ARGS__)

#define CLI_NO_ARGUMENTS CLI_PRIVATE_NO_ARGUMENTS

#define CLI_COMMAND CLI_PRIVATE_COMMAND
#define CLI_COMMAND_REF CLI_PRIVATE_COMMAND_REF

#endif //CLI_INCLUDE


#if defined(CLI_IMPLEMENTATION) && !defined(CLI_PRIVATE_TYPES_DEFINED)
#define CLI_PRIVATE_TYPES_DEFINED

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

typedef enum {
    CLI_ARGUMENT_FLAG,
    CLI_ARGUMENT_OPTION,
    CLI_ARGUMENT_OPTION_LIST,
    CLI_ARGUMENT_POSITIONAL,
    CLI_ARGUMENT_POSITIONAL_LIST,
    CLI_ARGUMENT_KIND_COUNT
} Cli__ArgumentKind;

typedef struct {
    Cli__ArgumentKind kind;
    char short_name;
    char *long_name;
    char *label;
    char *description;
    bool required;
    Cli_Parser parse;
    void *destination;
} Cli__ArgumentSpec;

typedef struct Cli__Command Cli__Command;

typedef struct {
    size_t argument_count;
    void (*build_arguments)(Cli__ArgumentSpec *arguments, void *out);
    size_t (*select_command)(void *out, size_t *selected);
    Cli__Command *commands;
    size_t command_count;
    bool commands_required;
} Cli__Schema;

struct Cli__Command {
    char *name;
    char *description;
    Cli__Schema *schema;
    // Byte offset of cmd.field within the parent's struct.
    size_t offset;
};

#endif //CLI_IMPLEMENTATION && !CLI_PRIVATE_TYPES_DEFINED

#if defined(CLI_COMMANDS) && !defined(CLI_ARGUMENTS)
#define CLI_ARGUMENTS
#endif

#if defined(CLI_ARGUMENTS)
// _RAW indirection forces a, b to expand before concatenating
#define CLI_PRIVATE_JOIN_RAW(a, b) a##b
#define CLI_PRIVATE_JOIN(a, b) CLI_PRIVATE_JOIN_RAW(a, b)
#define CLI_PRIVATE_JOIN3_RAW(a, b, c) a##b##c
#define CLI_PRIVATE_JOIN3(a, b, c) CLI_PRIVATE_JOIN3_RAW(a, b, c)

#ifdef CLI_COMMAND_REF_NAME
    #define CLI_PRIVATE_TYPE CLI_PRIVATE_JOIN(CLI_COMMAND_REF_NAME, _Arguments)
    #define CLI_PRIVATE_ENUM CLI_PRIVATE_JOIN(CLI_COMMAND_REF_NAME, _CommandType)
    #define CLI_PRIVATE_CMD(field) CLI_PRIVATE_JOIN3(CLI_COMMAND_REF_NAME, _Command_, field)
    #define CLI_PRIVATE_SYMBOL(suffix) CLI_PRIVATE_JOIN3(cli__, CLI_COMMAND_REF_NAME, suffix)
    #define CLI_PRIVATE_CHILD_TYPE(field) CLI_PRIVATE_JOIN3(CLI_COMMAND_REF_NAME, _Arguments_, field)
#else
    #define CLI_PRIVATE_TYPE Cli_Arguments
    #define CLI_PRIVATE_ENUM Cli_CommandType
    #define CLI_PRIVATE_CMD(field) CLI_PRIVATE_JOIN(Cli_Command_, field)
    #define CLI_PRIVATE_SYMBOL(suffix) CLI_PRIVATE_JOIN(cli__root, suffix)
    #define CLI_PRIVATE_CHILD_TYPE(field) CLI_PRIVATE_JOIN(Cli_Arguments_, field)
#endif //CLI_COMMAND_REF_NAME

#define CLI_PRIVATE_INLINE_BUILD_ARGUMENTS(field) CLI_PRIVATE_JOIN(CLI_PRIVATE_SYMBOL(_build_arguments_), field)
#define CLI_PRIVATE_INLINE_SCHEMA(field) CLI_PRIVATE_JOIN(CLI_PRIVATE_SYMBOL(_schema_), field)

#if defined(CLI_COMMAND_REF_NAME) || !defined(CLI_SCHEMA_DEFINED)
#define CLI_PRIVATE_NO_ARGUMENTS unsigned char __cli_unused;
#define CLI_PRIVATE_ARG(kind, type, field, ...) type field;

#ifdef CLI_COMMANDS
#define CLI_PRIVATE_COMMAND(field, name, description, arguments) \
    typedef struct { arguments } CLI_PRIVATE_CHILD_TYPE(field);
#define CLI_PRIVATE_COMMAND_REF(field, name, description, schema)

CLI_COMMANDS

#undef CLI_PRIVATE_COMMAND
#undef CLI_PRIVATE_COMMAND_REF

#define CLI_PRIVATE_COMMAND(field, ...) CLI_PRIVATE_CMD(field),
#define CLI_PRIVATE_COMMAND_REF CLI_PRIVATE_COMMAND

typedef enum {
    CLI_PRIVATE_CMD(none), // cmd none
    CLI_COMMANDS
} CLI_PRIVATE_ENUM;

#undef CLI_PRIVATE_COMMAND
#undef CLI_PRIVATE_COMMAND_REF
#endif //CLI_COMMANDS

#ifdef CLI_COMMAND_REF_NAME
typedef struct CLI_PRIVATE_TYPE CLI_PRIVATE_TYPE;
#endif

struct CLI_PRIVATE_TYPE {
    CLI_ARGUMENTS

#ifdef CLI_COMMANDS
    CLI_PRIVATE_ENUM cmd_type;

    union {
#define CLI_PRIVATE_COMMAND(field, name, description, arguments) CLI_PRIVATE_CHILD_TYPE(field) field;
#define CLI_PRIVATE_COMMAND_REF(field, name, description, schema) CLI_PRIVATE_JOIN(schema, _Arguments) field;

        CLI_COMMANDS

#undef CLI_PRIVATE_COMMAND
#undef CLI_PRIVATE_COMMAND_REF
    } cmd;
#endif //CLI_COMMANDS
};

#undef CLI_PRIVATE_ARG
#undef CLI_PRIVATE_NO_ARGUMENTS

#ifndef CLI_COMMAND_REF_NAME
#define CLI_SCHEMA_DEFINED
#endif
#endif //CLI_COMMAND_REF_NAME || !CLI_SCHEMA_DEFINED

#if defined(CLI_IMPLEMENTATION) && (defined(CLI_COMMAND_REF_NAME) || !defined(CLI_ROOT_GENERATED))

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

#define CLI_PRIVATE_NO_ARGUMENTS
#define CLI_PRIVATE_ARG(category, type, field, parser, ...) { \
    struct { type default_value; CLI_PRIVATE_##category##_FIELDS } argument_settings = {__VA_ARGS__}; \
    \
    if (out != NULL) \
        out->field = argument_settings.default_value; \
    \
    arguments[index++] = (Cli__ArgumentSpec) { \
        .kind = CLI_ARGUMENT_##category, .parse = (parser), \
        CLI_PRIVATE_##category##_SETTINGS(argument_settings), \
        .destination = out != NULL ? &out->field : NULL \
    }; \
}

static void CLI_PRIVATE_SYMBOL(_build_arguments)(Cli__ArgumentSpec *arguments, void *destination) {
    CLI_PRIVATE_TYPE *out = destination;
    size_t index = 0;

    CLI_ARGUMENTS

    (void)arguments; (void)out; (void)index;
}

#ifdef CLI_COMMANDS
#define CLI_PRIVATE_COMMAND(field, name, description, arguments_schema) \
    static void CLI_PRIVATE_INLINE_BUILD_ARGUMENTS(field)(Cli__ArgumentSpec *arguments, void *destination) { \
        CLI_PRIVATE_CHILD_TYPE(field) *out = destination; \
        size_t index = 0; \
        \
        arguments_schema \
        \
        (void)arguments; (void)out; (void)index; \
    }
#define CLI_PRIVATE_COMMAND_REF(field, name, description, schema)

CLI_COMMANDS

#undef CLI_PRIVATE_COMMAND
#undef CLI_PRIVATE_COMMAND_REF

static size_t CLI_PRIVATE_SYMBOL(_select_command)(void *destination, size_t *selected) {
    CLI_PRIVATE_TYPE *out = destination;

    if (selected)
        out->cmd_type = (CLI_PRIVATE_ENUM)*selected;

    return (size_t)out->cmd_type;
}
#endif //CLI_COMMANDS
#undef CLI_PRIVATE_ARG

#define CLI_PRIVATE_ARG(...) + 1

#ifdef CLI_COMMANDS
#define CLI_PRIVATE_COMMAND(field, name, description, arguments) \
    static Cli__Schema CLI_PRIVATE_INLINE_SCHEMA(field) = { \
        .argument_count = 1 arguments, .build_arguments = CLI_PRIVATE_INLINE_BUILD_ARGUMENTS(field) \
    };

#define CLI_PRIVATE_COMMAND_REF(field, name, description, schema)

CLI_COMMANDS

#undef CLI_PRIVATE_COMMAND
#undef CLI_PRIVATE_COMMAND_REF

#define CLI_PRIVATE_COMMAND(field, command_name, text, arguments) \
    {.name = command_name, .description = text, \
     .schema = &CLI_PRIVATE_INLINE_SCHEMA(field), \
     .offset = offsetof(CLI_PRIVATE_TYPE, cmd.field)},

#define CLI_PRIVATE_COMMAND_REF(field, command_name, text, schema_name) \
    {.name = command_name, .description = text, \
     .schema = &CLI_PRIVATE_JOIN3(cli__, schema_name, _schema), \
     .offset = offsetof(CLI_PRIVATE_TYPE, cmd.field)},

static Cli__Command CLI_PRIVATE_SYMBOL(_commands)[] = {
    {0},
    CLI_COMMANDS
};

#undef CLI_PRIVATE_COMMAND
#undef CLI_PRIVATE_COMMAND_REF
#endif //CLI_COMMANDS

static Cli__Schema CLI_PRIVATE_SYMBOL(_schema) = {
    .argument_count = 1 CLI_ARGUMENTS,
    .build_arguments = CLI_PRIVATE_SYMBOL(_build_arguments),
#ifdef CLI_COMMANDS
    .select_command = CLI_PRIVATE_SYMBOL(_select_command),
    .commands = CLI_PRIVATE_SYMBOL(_commands),
    .command_count = sizeof(CLI_PRIVATE_SYMBOL(_commands)) / sizeof(Cli__Command),
#ifdef CLI_COMMANDS_REQUIRED
    .commands_required = CLI_COMMANDS_REQUIRED,
#else
    .commands_required = true,
#endif
#endif //CLI_COMMANDS
};

#undef CLI_PRIVATE_ARG
#undef CLI_PRIVATE_NO_ARGUMENTS
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

#ifndef CLI_COMMAND_REF_NAME
#define CLI_ROOT_GENERATED
#endif
#endif //CLI_IMPLEMENTATION && (CLI_COMMAND_REF_NAME || !CLI_ROOT_GENERATED)

// Cleanup for child schema includes
#undef CLI_PRIVATE_TYPE
#undef CLI_PRIVATE_ENUM
#undef CLI_PRIVATE_CMD
#undef CLI_PRIVATE_SYMBOL
#undef CLI_PRIVATE_CHILD_TYPE
#undef CLI_PRIVATE_JOIN
#undef CLI_PRIVATE_JOIN_RAW
#undef CLI_PRIVATE_JOIN3
#undef CLI_PRIVATE_JOIN3_RAW
#undef CLI_PRIVATE_INLINE_BUILD_ARGUMENTS
#undef CLI_PRIVATE_INLINE_SCHEMA

#ifdef CLI_COMMAND_REF_NAME
#undef CLI_COMMAND_REF_NAME
#undef CLI_ARGUMENTS
#undef CLI_COMMANDS
#undef CLI_COMMANDS_REQUIRED
#endif

#endif //CLI_ARGUMENTS

#if defined(CLI_IMPLEMENTATION) && defined(CLI_ROOT_GENERATED) && !defined(CLI_IMPLEMENTATION_INCLUDED)
#define CLI_IMPLEMENTATION_INCLUDED

#ifndef CLI_BOLD
#define CLI_BOLD(text) "\033[1m" text "\033[0m"
#endif
#ifndef CLI_UNDERLINE
#define CLI_UNDERLINE(text) "\033[4m" text "\033[0m"
#endif

static Cli_ParseStatus cli__parse_schema(Cli__Schema *schema, int argc, char *argv[], void *out,
                                   void *context, Cli_ParseError *error);
static Cli_ParseStatus cli__parse_tokens(size_t count, Cli__ArgumentSpec arguments[], bool seen[], void *context,
                                   int argc, char *argv[], int *command_position, Cli_ParseError *error);
#ifndef CLI_NDEBUG
static Cli_ParseStatus cli__validate_schema(Cli__Schema *schema, Cli__ArgumentSpec arguments[], Cli_ParseError *error);
#endif
static Cli__Schema *cli__select_schema(void *out, FILE *path);
static void cli__build_schema(Cli__Schema *schema, Cli__ArgumentSpec arguments[], void *out);
static bool cli__is_positional(Cli__ArgumentSpec *argument);
static Cli_ParseStatus cli__set_error(Cli_ParseError *error, char *input, char *message);
static Cli_ParseStatus cli__set_argument_error(Cli_ParseError *error, Cli__ArgumentSpec *argument,
                                             char *input, char *message);

void cli_print_usage(FILE *stream, char *program, Cli_Arguments *args) {
    fprintf(stream, "Usage: %s", program);
    Cli__Schema *schema = cli__select_schema(args, stream);
    size_t count = schema->argument_count;
    Cli__ArgumentSpec arguments[count];

    cli__build_schema(schema, arguments, NULL);

    bool short_flags = false;
    for (size_t i = 0; i < count; ++i) {
        Cli__ArgumentSpec *argument = &arguments[i];
        if (argument->kind != CLI_ARGUMENT_FLAG || !argument->short_name || argument->required)
            continue;

        if (!short_flags) fputs(" [-", stream);

        fputc(argument->short_name, stream);
        short_flags = true;
    }
    if (short_flags) fputc(']', stream);

    for (Cli__ArgumentKind kind = CLI_ARGUMENT_FLAG; kind < CLI_ARGUMENT_KIND_COUNT; ++kind) {
        for (size_t i = 0; i < count; ++i) {
            Cli__ArgumentSpec *argument = &arguments[i];

            if (argument->kind != kind) continue;
            if (kind == CLI_ARGUMENT_FLAG && argument->short_name && !argument->required) continue;

            fputc(' ', stream);
            if (!argument->required) fputc('[', stream);

            if (argument->short_name) fprintf(stream, "-%c", argument->short_name);
            else if (argument->long_name) fprintf(stream, "--%s", argument->long_name);

            if (argument->kind != CLI_ARGUMENT_FLAG) {
                char *label = argument->label ? argument->label : "VALUE";
                if (!cli__is_positional(argument)) fputc(' ', stream);
                fprintf(stream, "%s", label);
                if (kind == CLI_ARGUMENT_POSITIONAL_LIST) fputs("...", stream);
            }

            if (!argument->required) fputc(']', stream);
        }
    }

    if (schema->command_count > 1) {
        char *message = schema->commands_required 
                        ? " COMMAND [ARGS...]" 
                        : " [COMMAND [ARGS...]]";
        fputs(message, stream);
    }

    fputc('\n', stream);
}

void cli_print_help(FILE *stream, Cli_Arguments *args) {
    Cli__Schema *schema = cli__select_schema(args, NULL);
    size_t count = schema->argument_count;
    Cli__ArgumentSpec arguments[count];

    cli__build_schema(schema, arguments, NULL);
    fputs("Arguments:\n", stream);

    for (Cli__ArgumentKind kind = CLI_ARGUMENT_FLAG; kind < CLI_ARGUMENT_KIND_COUNT; ++kind) {
        for (size_t i = 0; i < count; ++i) {
            Cli__ArgumentSpec *argument = &arguments[i];
            if (argument->kind != kind) continue;

            char *label = argument->label ? argument->label : "value";
            fputs("  ", stream);

            if (argument->short_name) {
                fprintf(stream, CLI_BOLD("-%c"), argument->short_name);
                if (argument->kind != CLI_ARGUMENT_FLAG)
                    fprintf(stream, " " CLI_UNDERLINE("%s"), label);
            }

            if (argument->long_name) {
                if (argument->short_name) fputs(", ", stream);
                fprintf(stream, CLI_BOLD("--%s"), argument->long_name);
                if (argument->kind != CLI_ARGUMENT_FLAG)
                    fprintf(stream, "=" CLI_UNDERLINE("%s"), label);
            }

            if (cli__is_positional(argument)) {
                fprintf(stream, CLI_UNDERLINE("%s"), label);
                if (kind == CLI_ARGUMENT_POSITIONAL_LIST) fputs("...", stream);
            }

            if (argument->required) fputs(" (required)", stream);
            if (argument->kind == CLI_ARGUMENT_OPTION_LIST) fputs(" (repeatable)", stream);
            fputc('\n', stream);

            if (argument->description != NULL) fprintf(stream, "      %s\n", argument->description);
            fputc('\n', stream);
        }
    }

    if (schema->command_count > 1) {
        fputs("Commands:\n", stream);
        for (size_t i = 1; i < schema->command_count; ++i) {
            Cli__Command *command = &schema->commands[i];
            fprintf(stream, "  " CLI_BOLD("%s") "\n", command->name);
            if (command->description) fprintf(stream, "      %s\n", command->description);
            fputc('\n', stream);
        }
    }
}

static Cli__Schema *cli__select_schema(void *out, FILE *path) {
    Cli__Schema *schema = &cli__root_schema;

    while (out && schema->select_command) {
        size_t selected = schema->select_command(out, NULL);
        if (selected == 0 || selected >= schema->command_count) break;

        Cli__Command *command = &schema->commands[selected];
        if (path) fprintf(path, " %s", command->name);

        out = (char *)out + command->offset;
        schema = command->schema;
    }

    return schema;
}

Cli_ParseStatus cli_parse(int argc, char *argv[], Cli_Arguments *out, void *context, Cli_ParseError *error) {
    *out = (Cli_Arguments) {0};
    *error = (Cli_ParseError) {0};
    return cli__parse_schema(&cli__root_schema, argc, argv, out, context, error);
}

static Cli_ParseStatus cli__parse_schema(Cli__Schema *schema, int argc, char *argv[], void *out,
                                   void *context, Cli_ParseError *error) {
    size_t count = schema->argument_count;
    Cli__ArgumentSpec arguments[count];
    bool seen[count];
    memset(seen, 0, sizeof(seen));
    bool help_requested = false;
    size_t selected = 0;
    if (schema->select_command) schema->select_command(out, &selected);

    cli__build_schema(schema, arguments, out);
    arguments[count - 1].destination = &help_requested;

#ifndef CLI_NDEBUG
    Cli_ParseStatus valid = cli__validate_schema(schema, arguments, error);
    if (valid != CLI_OK) return valid;
#endif

    int command_position = argc;
    Cli_ParseStatus status = cli__parse_tokens(count, arguments, seen, context, argc, argv,
                                         schema->command_count > 1 ? &command_position : NULL, error);
    if (help_requested) return CLI_HELP;
    if (status != CLI_OK) return status;

    if (schema->commands_required && command_position == argc)
        return cli__set_error(error, NULL, "command is required");

    if (command_position < argc) {
        size_t command = 1;
        while (command < schema->command_count &&
               strcmp(argv[command_position], schema->commands[command].name) != 0)
        {
            ++command;
        }

        if (command == schema->command_count)
            return cli__set_error(error, argv[command_position], "unknown command");

        schema->select_command(out, &command);
        Cli__Command *child = &schema->commands[command];
        status = cli__parse_schema(child->schema, argc - command_position, argv + command_position,
                                  (char *)out + child->offset, context, error);

        if (status != CLI_OK) return status;
    }

    for (size_t i = 0; i < count; ++i) {
        if (arguments[i].required && !seen[i]) {
            selected = 0;
            if (schema->select_command) schema->select_command(out, &selected);
            return cli__set_argument_error(error, &arguments[i], NULL, "required argument is missing");
        }
    }
    return CLI_OK;
}

static Cli_ParseStatus cli__parse_tokens(size_t count, Cli__ArgumentSpec arguments[], bool seen[], void *context,
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

                while (positional < count && !cli__is_positional(&arguments[positional]))
                    ++positional;

                if (positional < count) {
                    match = positional;
                    // Skip over "regular" positional args, stay on list
                    if (arguments[positional].kind == CLI_ARGUMENT_POSITIONAL) ++positional;
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
                if (named && !long_option) {
                    error->input = token;
                    snprintf(error->message, sizeof(error->message), "unknown option: -%c", short_cursor[-1]);
                    return CLI_ERROR;
                }

                char *message = named ? "unknown option" : "unexpected positional argument";
                return cli__set_error(error, token, message);
            }

            Cli__ArgumentSpec *argument = &arguments[match];
            if (seen[match] && argument->kind != CLI_ARGUMENT_OPTION_LIST
                 && argument->kind != CLI_ARGUMENT_POSITIONAL_LIST)
                return cli__set_argument_error(error, argument, token, "argument is duplicated");

            char *value = NULL;
            if (argument->kind != CLI_ARGUMENT_FLAG) {
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
                    return cli__set_argument_error(error, argument, NULL, "option requires a value");

                // Short option is consumed fully or it is NULL from the start
                short_cursor = NULL;
            }
            else if (equals != NULL) {
                return cli__set_argument_error(error, argument, equals + 1, "flag does not take a value");
            }

            *error = (Cli_ParseError) {0};
            Cli_ParseStatus status = argument->parse(value, argument->destination, context, error);
            if (status != CLI_OK) {
                cli__set_argument_error(error, argument, 
                    error->input ? error->input : value,
                    error->message[0] ? error->message : "invalid argument value"
                );

                return status;
            }

            seen[match] = true;
        } while (short_cursor != NULL && *short_cursor != '\0');
    }

    return CLI_OK;
}

#ifndef CLI_NDEBUG
static Cli_ParseStatus cli__validate_schema(Cli__Schema *schema, Cli__ArgumentSpec arguments[], Cli_ParseError *error) {
    size_t count = schema->argument_count;
    bool optional_positional = false;
    bool positional_list = false;

    for (size_t i = 0; i < count; ++i) {
        Cli__ArgumentSpec *argument = &arguments[i];

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
            if (schema->command_count > 1) message = "commands cannot be combined with positional arguments";
            if (positional_list) message = "a positional list must be the last positional argument";
            if (argument->kind == CLI_ARGUMENT_POSITIONAL_LIST) {
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
            return cli__set_argument_error(error, argument, NULL, message);
        }
    }

    for (size_t i = 1; i < schema->command_count; ++i) {
        char *name = schema->commands[i].name;
        if (!name || !name[0] || name[0] == '-')
            return cli__set_error(error, name, "invalid command name");
        for (char *p = name; *p; ++p)
            if (isspace((unsigned char)*p)) return cli__set_error(error, name, "invalid command name");
        for (size_t j = 1; j < i; ++j)
            if (strcmp(name, schema->commands[j].name) == 0)
                return cli__set_error(error, name, "duplicate command name");
    }

    return CLI_OK;
}
#endif //CLI_NDEBUG

static void cli__build_schema(Cli__Schema *schema, Cli__ArgumentSpec arguments[], void *out) {
    schema->build_arguments(arguments, out);
    arguments[schema->argument_count - 1] = (Cli__ArgumentSpec) {
        .kind = CLI_ARGUMENT_FLAG, .short_name = 'h', .long_name = "help", .description = "Show usage",
        .parse = cli_parse_flag
    };
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

Cli_ParseStatus cli_parse_f64(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    double number = strtod(input, &end);
    if (end == input || *end != '\0' || errno == ERANGE || !isfinite(number))
        return cli__set_error(error, input, "invalid or out-of-range floating-point number");

    *(double *)destination = number;
    return CLI_OK;
}

static bool cli__is_positional(Cli__ArgumentSpec *argument) {
    return argument->kind == CLI_ARGUMENT_POSITIONAL || argument->kind == CLI_ARGUMENT_POSITIONAL_LIST;
}

static Cli_ParseStatus cli__set_error(Cli_ParseError *error, char *input, char *message) {
    error->input = input;
    snprintf(error->message, sizeof(error->message), "%s", message);
    return CLI_ERROR;
}

static Cli_ParseStatus cli__set_argument_error(Cli_ParseError *error, Cli__ArgumentSpec *argument,
                                             char *input, char *message) {
    Cli_ParseError result = {.input = input};
    if (argument->long_name)
        snprintf(result.message, sizeof(result.message), "%s: --%s", message, argument->long_name);
    else if (argument->short_name)
        snprintf(result.message, sizeof(result.message), "%s: -%c", message, argument->short_name);
    else {
        char *arg = argument->label
                    ? argument->label
                    : "argument";
        snprintf(result.message, sizeof(result.message), "%s: %s", message, arg);
    }

    *error = result;
    return CLI_ERROR;
}

#endif //CLI_IMPLEMENTATION && CLI_ROOT_GENERATED && !CLI_IMPLEMENTATION_INCLUDED
