#include "vendor/psh_core/psh_core.h"
#include "cli.h"
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>

#define CLI_BOLD(text) "\033[1m" text "\033[0m"
#define CLI_UNDERLINE(text) "\033[4m" text "\033[0m"

static Cli_ParseStatus set_error(Cli_ParseError *error, char *input, char *message) {
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
        return set_error(error, input, "invalid or out-of-range integer");

    *(i64 *)destination = (i64)number;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_u64(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    uintmax_t number = strtoumax(input, &end, 10);
    if (strchr(input, '-') || end == input || *end != '\0' || errno == ERANGE || number > UINT64_MAX)
        return set_error(error, input, "invalid or out-of-range unsigned integer");

    *(u64 *)destination = (u64)number;
    return CLI_OK;
}

Cli_ParseStatus cli_parse_double(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    char *end;
    errno = 0;
    f64 number = strtod(input, &end);
    if (end == input || *end != '\0' || errno == ERANGE || !isfinite(number))
        return set_error(error, input, "invalid or out-of-range floating-point number");

    *(f64 *)destination = number;
    return CLI_OK;
}

Cli_ParseStatus cli_internal_parse(Cli_ArgumentSpec *arguments, bool *seen, usize count, void *context,
                                   i32 argc, char *argv[], Cli_ParseError *error) {
#ifndef NDEBUG
    bool optional_positional = false;
    for (usize i = 0; i < count; ++i) {
        Cli_ArgumentSpec *argument = &arguments[i];
        seen[i] = false;

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
                if (*p == '=' || isspace((u8)*p)) message = "invalid long option name";
        }

        if (!argument->short_name && !argument->long_name) {
            if (!argument->label) message = "positional argument must have a label";
            if (argument->value_mode == CLI_NO_VALUE) message = "positional argument must take a value";
            if (argument->required && optional_positional) message = "required positional argument follows an optional one";
            if (!argument->required) optional_positional = true;
        }

        for (usize j = 0; j < i; ++j) {
            if ((argument->short_name && argument->short_name == arguments[j].short_name)
                || (argument->long_name && arguments[j].long_name
                    && strcmp(argument->long_name, arguments[j].long_name) == 0))
                message = "duplicate option name";
        }

        if (message != NULL)
            return set_error(error, argument->long_name ? argument->long_name : argument->label, message);
    }
#endif //NDEBUG

    bool options = true;
    usize positional = 0;
    for (i32 i = 1; i < argc; ++i) {
        char *token = argv[i];
        if (options && strcmp(token, "--") == 0) {
            options = false;
            continue;
        }
        bool named = options && token[0] == '-' && token[1] != '\0';
        bool long_option = named && token[1] == '-';
        char *short_option = named && !long_option ? token + 1 : NULL;
        char *equals = long_option ? strchr(token + 2, '=') : NULL;
        usize name_length = long_option ? (equals ? (usize)(equals - token - 2) : strlen(token + 2)) : 0;

        do {
            usize match = count;
            if (named) {
                for (usize j = 0; j < count; ++j) {
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
                while (positional < count && (arguments[positional].short_name || arguments[positional].long_name))
                    ++positional;
                if (positional < count) match = positional++;
            }
            if (match == count)
                return set_error(error, token,
                                 named ? "unknown option" : "unexpected positional argument");

            Cli_ArgumentSpec *argument = &arguments[match];
            if (seen[match])
                return set_error(error, token, "argument was already supplied");
            char *value = NULL;
            if (argument->value_mode == CLI_VALUE_REQUIRED) {
                if (!named) value = token;
                else if (equals != NULL) value = equals + 1;
                else if (short_option != NULL && *short_option != '\0') value = short_option;
                else if (i + 1 < argc) value = argv[++i];
                else return set_error(error, token, "option requires a value");
                short_option = NULL;
            } else if (equals != NULL) {
                return set_error(error, token, "flag does not take a value");
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

static ArgumentGroup argument_group(Cli_ArgumentSpec *argument) {
    if (!argument->short_name && !argument->long_name) return ARGUMENT_GROUP_POSITIONAL;
    return argument->value_mode == CLI_NO_VALUE ? ARGUMENT_GROUP_FLAG : ARGUMENT_GROUP_OPTION;
}

void cli_internal_print_usage(Cli_ArgumentSpec *arguments, usize count, FILE *stream, char *program) {
    fprintf(stream, "Usage: %s", program);

    bool short_flags = false;
    for (usize i = 0; i < count; ++i) {
        Cli_ArgumentSpec *argument = &arguments[i];
        if (argument->value_mode != CLI_NO_VALUE || !argument->short_name || argument->required) continue;
        if (!short_flags) fputs(" [-", stream);
        fputc(argument->short_name, stream);
        short_flags = true;
    }
    if (short_flags) fputc(']', stream);

    for (ArgumentGroup group = ARGUMENT_GROUP_FLAG; group < ARGUMENT_GROUP_COUNT; ++group) {
        for (usize i = 0; i < count; ++i) {
            Cli_ArgumentSpec *argument = &arguments[i];

            if (argument_group(argument) != group) continue;
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

void cli_internal_print_help(Cli_ArgumentSpec *arguments, usize count, FILE *stream) {
    fputs("Arguments:\n", stream);

    for (ArgumentGroup group = ARGUMENT_GROUP_FLAG; group < ARGUMENT_GROUP_COUNT; ++group) {
        for (usize i = 0; i < count; ++i) {
            Cli_ArgumentSpec *argument = &arguments[i];
            if (argument_group(argument) != group) continue;

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
