# libcli

libcli is a single-header C99 library that generates a command-line interface from an argument schema.
Declare flags, options, and positional arguments once, then libcli generates typed result structs and the code for parsing arguments and printing usage and help.

## Setup

Copy `cli.h` into your project. Declare fields with `CLI_ARGUMENTS` and define
`CLI_IMPLEMENTATION` in exactly one C file:

```c
#define CLI_IMPLEMENTATION
#define CLI_ARGUMENTS \
    CLI_ARG_OPTION_U64(count, .short_name = 'c', .default_value = 1, .label = "NUM") \
    CLI_ARG_POSITIONAL_TEXT(host, .required = true, .label = "HOST")
#include "cli.h"

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
        return 1;
    }

    printf("%s: %llu\n", args.host, (unsigned long long)args.count);
    return 0;
}
```

Build with `cc -std=c99 example.c` and run `./a.out -c 3 localhost`. 

## Other Examples

- [Commands](examples/commands/main.c): typed subcommands and flags.
- [Timeout](examples/timeout/README.md): custom parsers and a trailing argument list.


## Public API

| Schema macro | Purpose |
| --- | --- |
| `CLI_ARG_FLAG(field, ...)` | Boolean option |
| `CLI_ARG_OPTION(type, field, parser, ...)` | Named option with one value |
| `CLI_ARG_OPTION_LIST(type, field, parser, ...)` | Repeated named option |
| `CLI_ARG_POSITIONAL(type, field, parser, ...)` | One positional value |
| `CLI_ARG_POSITIONAL_LIST(type, field, parser, ...)` | Trailing positional values |
| `CLI_ARG_OPTION_<TYPE>(field, ...)` | Typed option shortcut |
| `CLI_ARG_POSITIONAL_<TYPE>(field, ...)` | Typed positional shortcut |
| `CLI_NO_ARGUMENTS` | Explicitly empty root or command schema |
| `CLI_COMMAND(field, name, description, arguments)` | Subcommand |

`<TYPE>` is `TEXT`, `I64`, `U64`, or `F64`, using borrowed `char *`, `int64_t`,
`uint64_t`, or `double`, respectively.

Named arguments take `.short_name`, `.long_name`, or both. `.label` names a value
and is required for positionals. All support `.description`, `.required`, and a
typed `.default_value` initializer. If not provided otherwise, the default values are zero initialized.

### Subcommands

Define `CLI_COMMANDS` before including the library:

```c
#define CLI_COMMANDS \
    CLI_COMMAND(send, "send", "Send a file", SEND_ARGUMENTS) \
    CLI_COMMAND(status, "status", "Show status", CLI_NO_ARGUMENTS)

#define SEND_ARGUMENTS \
    CLI_ARG_POSITIONAL_TEXT(file, .required = true, .label = "FILE")
```

Inside the `Cli_Arguments` struct, this creates a union member `cmd` and an enum `cmd_type`. Use the `cmd_type` member to access the correct subcommand's parsed values.

### Configuration

Define configuration macros before the implementation include:

- `CLI_COMMANDS_REQUIRED`: defaults to `true`, which means that all commands are by default treated as required arguments. Set to `false` to allow root-only calls. With no command, `cmd_type` is `cli_cmd_none`.
- `CLI_NDEBUG`: omit developer schema validation.
- `CLI_BOLD(text)` and `CLI_UNDERLINE(text)`: override ANSI help styling. They must
  expand to concatenatable string literals. To disable styling:

  ```c
  #define CLI_BOLD(text) text
  #define CLI_UNDERLINE(text) text
  ```

## Parsing behavior

- Flags take no value. `-v` or `--verbose` sets a boolean field to `true`.
  Short flags can be grouped: `-abc` means `-a -b -c`.
- Options take one value. For a count option, write `-c 3`, `-c3`,
  `--count 3`, or `--count=3`. A value is required whenever the option appears.
- Positional arguments are assigned in declaration order.
  In `app input.txt output.txt`, the first value goes to the first positional
  field and the second goes to the next. Declare required positional arguments
  before optional ones.
- Flags and options can appear before or between positional arguments, but must
  come before a positional list begins.
- Option lists allow repetition: `--include src --include tests` calls the
  list parser once for `src` and once for `tests`. Other options and flags cannot
  be repeated.
- A positional list takes everything left. Once its first value is read, all
  remaining arguments belong to it, including `--help` and `--`. Put at most one
  positional list last; optional positional arguments take their values before
  the list. A required list needs at least one value.
- `--` makes following arguments positional values, even if they start with `-`:
  `app -- -filename`. A lone `-` is also a positional value.
- Unknown options and extra positional arguments are errors. A default does not
  satisfy the requirement to provide an argument marked `.required = true`.
- `-h` and `--help` are provided automatically. Help works without supplying
  required arguments, but an error encountered before the help flag still fails.

## Custom parsers

Use `CLI_ARG_OPTION` or `CLI_ARG_POSITIONAL` (or their `_LIST` versions respectively) when a field needs your own C type
or validation rules. Provide a function that converts one argument's text into
that field. The function receives:

- `input`: the value alone, such as `"json"` from `--format json`.
- `destination`: a pointer to the field in `Cli_Arguments`; cast it to a pointer
  to your type, such as `OutputFormat *` below.
- `context`: the pointer passed as the fourth argument to `cli_parse`, or `NULL`
  when no extra data is needed.
- `error`: where you write a reason if the value is invalid.

For example, parse an output format into an enum:

```c
#define CLI_IMPLEMENTATION
#include "cli.h"
#include <string.h>

typedef enum { FORMAT_TEXT, FORMAT_JSON } OutputFormat;

static Cli_ParseStatus parse_format(char *input, void *destination, void *context, Cli_ParseError *error) {
    (void)context;
    OutputFormat format;
    if (strcmp(input, "text") == 0) format = FORMAT_TEXT;
    else if (strcmp(input, "json") == 0) format = FORMAT_JSON;
    else {
        snprintf(error->message, sizeof(error->message), "expected text or json");
        return CLI_ERROR;
    }
    *(OutputFormat *)destination = format;
    return CLI_OK;
}

#define CLI_ARGUMENTS \
    CLI_ARG_OPTION(OutputFormat, format, parse_format, .long_name = "format", \
        .default_value = FORMAT_TEXT, .label = "FORMAT")
#include "cli.h"
```

The first include makes the library types available to the callback. The second
uses the schema to generate the argument struct and implementation. Call
`cli_parse` as in Setup: `--format json` gives `args.format == FORMAT_JSON`;
omitting the option uses `FORMAT_TEXT`.

Return `CLI_OK` after writing the field, or `CLI_ERROR` with a null-terminated
message in `error->message`. The library adds the argument name and supplies the
failing input automatically; the callback should not print or exit. Built-in
parsers `cli_parse_flag`, `cli_parse_text`, `cli_parse_i64`, `cli_parse_u64`, and
`cli_parse_f64` have the same signature and can be reused.

For lists, the callback receives the same field for each value. Use a struct
holding an array and a length; append each value and report an error when full or perform array resizing.
Storage and capacity checks belong to your code. Text values borrow `argv`
strings, so they stay valid only while those strings remain available.

## Limitations

- One root schema per program and one level of subcommands.
- Root options precede the command; root positionals cannot coexist with commands.
- Defaults are runtime expressions, evaluated during parsing and again when
  printing usage or help. Prefer expressions without side effects.
- Help does not print default values or detect terminals automatically.

## License

[MIT](LICENSE).
