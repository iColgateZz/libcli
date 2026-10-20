# timeout

A small C99 example of `cli.h`. It runs a command, polls `waitpid` with
`WNOHANG`, and sends a signal if the time limit expires.

## Build and run

Requires a C compiler and Make on macOS or Linux:

```sh
make
./build/timeout --help
./build/timeout 2 sleep 1
./build/timeout 0.1 sleep 2
./build/timeout -s 9 0.1 sh -c 'exec sleep 2'
```

```text
timeout [-s SIG] SECS COMMAND [ARGS...]
```

- `-h`, `--help`: print usage and help to stdout.
- `-s SIG`: select a valid positive signal number, default `15` (`SIGTERM`).
  For example, `-s 9` sends `SIGKILL`. Names such as `TERM` are not accepted.
  Signal numbers depend on the OS; use `kill -l` to list them.
- `SECS`: a finite, nonnegative number of seconds, including fractions. Zero
  disables the time limit. Unit suffixes are not supported.
- `COMMAND`: required executable, resolved through `PATH`.
- `ARGS...`: optional arguments passed unchanged to the executable.

Timeout options must precede `COMMAND`. Once the command is encountered,
everything after it is passed through, including `--help` and `--`. Use `--`
before a command name beginning with `-`.

The command and its arguments form one required positional list. Requiring the
list only requires its first element (the command), not any additional arguments.
Its callback appends borrowed strings to a `Timeout_Arguments` structure with
`items`, `size`, and `capacity`. Static storage holds the command plus up to 255
arguments and the null terminator required by `execvp`. No heap allocation is
performed by the example.

## Behavior

`main.c` defines the schema, validates inputs, and handles help/errors.
`timeout.c` handles `fork`, `execvp`, timing, signaling, and child reaping.
`timeout.h` declares the runner interface; `list.h` provides fixed-storage list
macros without third-party types or allocation.

The runner checks the child approximately every 10 milliseconds using
`CLOCK_MONOTONIC` and `nanosleep`. Near the deadline it shortens the sleep.
Scheduling can delay checks; this is not a real-time deadline guarantee.
After sending the selected signal it waits for the child to exit.

Only the direct child is signaled. Descendants are not managed. Signals that
are caught, ignored, or do not terminate the command can leave timeout waiting
indefinitely; select `-s 9` to force termination of the direct child.
There is no automatic escalation or signal-forwarding handler. The child shares
the caller's terminal and process group.

The command inherits stdin, stdout, and stderr. Timeout itself prints diagnostics
only to stderr. If the command exits normally before the deadline, its exit status
is preserved. Invalid arguments, runner errors, execution failures, timeouts, and
signal termination use `-1`, which appears to the shell as exit status `255` on
macOS and Linux. Help returns `0`.

A command can itself return `255`, so that status is not unique to timeout errors.
This is a teaching
example, not a complete implementation of GNU
[`timeout(1)`](https://www.man7.org/linux/man-pages/man1/timeout.1.html).

## Platforms

Verified on macOS 26.3 and Debian GNU/Linux 13 (trixie) in OrbStack.

From the repository root, Debian can build in a separate output directory:

```sh
orb make -C examples/timeout BUILD_DIR=build/debian
```

The example is covered by the root [MIT license](../../LICENSE).
