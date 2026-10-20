# cli.h

A single-header C99 argument parser.

## Example

The [timeout example](examples/timeout/README.md) runs a command with a time limit.
It demonstrates scalar arguments, a custom signal parser, and a trailing list
backed by fixed storage.

```sh
make -C examples/timeout
examples/timeout/build/timeout --help
examples/timeout/build/timeout 0.5 sleep 2
examples/timeout/build/timeout -s 9 1 sh -c 'exec sleep 2'
```

The example targets macOS and Linux. Verified on macOS 26.3 and
Debian GNU/Linux 13 (trixie) through OrbStack. See the example README for its
behavior, exit statuses, and limitations.

## Declaring arguments

See [examples/timeout/main.c](examples/timeout/main.c) for a complete schema and
application flow:

1. Define `CLI_IMPLEMENTATION` in one C file and include `cli.h` for its public
   declarations.
2. Declare custom types, storage, and parser callbacks.
3. Define `CLI_ARGUMENTS` using the macros below, then include `cli.h` again to
   generate the result structure and implementation.
4. Call `cli_parse(argc, argv, &args, context, &error)`. Handle `CLI_HELP`,
   `CLI_ERROR`, or use the populated `args` on `CLI_OK`.

| Macro | Purpose |
| --- | --- |
| `CLI_ARG_FLAG` | Boolean option |
| `CLI_ARG_TEXT`, `CLI_ARG_I64`, `CLI_ARG_U64`, `CLI_ARG_DOUBLE` | Named option with one typed value |
| `CLI_ARG` | Named option with a custom type and parser |
| `CLI_ARG_LIST` | Repeated named option; callback receives each value |
| `CLI_ARG_POSITIONAL` | One positional value |
| `CLI_ARG_POSITIONAL_LIST` | Trailing positional values; callback receives each token |

Named arguments accept `.short_name`, `.long_name`, or both. Value-taking
arguments accept `.label`. All kinds accept `.description`, `.required`, and a
typed `.default_value` C initializer. Defaults do not satisfy required presence.
Flags default to false; other omitted defaults are zero-initialized.

The library generates one schema per program. To share `Cli_ParsedArgs` across
source files, put the schema and its type dependencies in an application header
with an include guard. Include that header wherever its fields are needed;
define `CLI_IMPLEMENTATION` before including it in exactly one C file. Custom
parser functions used by that implementation must be declared before expansion.

## Parsing behavior

- Short flags can be grouped: `-abc`. Values may follow a short option directly
  or separately: `-c3`, `-c 3`. Long options accept `--count=3` and `--count 3`.
- `--` ends option recognition.
- Options can appear among scalar positional arguments.
- **Once a trailing positional list receives its first token, all remaining
  tokens belong to it**, including `-h`, `--help`, and `--`. Before the list
  starts, a token beginning with `-` still looks like an option; use `--` when
  the first list value begins with `-`.
- Scalars and flags cannot repeat. Named lists accept repeated occurrences.
- Required scalar positionals precede optional scalar positionals; at most one
  positional list may follow them. Optional scalars consume available positional
  tokens before the list. A required list needs at least one token.
- `-h` and `--help` are reserved. Encountered help bypasses required checks.
  Parsing stops at the first error, so help appearing after an error is not
  processed.
- Integers use decimal conversion; doubles use the C library's `strtod` and
  current locale. Conversions require a complete, in-range value; doubles must
  be finite. Leading whitespace is accepted by the numeric parsers.

Custom callbacks receive the input string, destination field, caller context,
and error output. Return `CLI_OK` or `CLI_ERROR`; on failure supply a useful
`error->message`. The library supplies the input and a fallback message if
omitted. Callbacks control list storage and capacity checks.

Each parse initializes fields from their defaults. Text values borrow `argv`
strings without modifying them. List storage belongs to the application.
Results may be partially updated on failure and should then be discarded.
Schema checks are enabled in debug builds and omitted with `NDEBUG`.

The parser neither prints nor exits. Call `cli_print_usage(stream, program)` and
`cli_print_help(stream)` explicitly. Help uses ANSI bold and underline sequences.
Default values are not automatically printed.

## License

The library and example are MIT licensed; see [LICENSE](LICENSE).
