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
  For example, `-s 9` sends `SIGKILL`. Signal numbers depend on the OS; use `kill -l` to list them.
- `SECS`: a finite, nonnegative number of seconds, including fractions. Zero
  disables the time limit. Unit suffixes are not supported.
- `COMMAND`: required executable, resolved through `PATH`.
- `ARGS...`: optional arguments passed unchanged to the executable.

Timeout options must precede `COMMAND`. Once the command is encountered,
everything after it is passed through, including `--help` and `--`. Use `--`
before a command name beginning with `-`.

## Platforms

Verified on macOS 26.3 and Debian GNU/Linux 13 (trixie) in OrbStack.


## License

The example is covered by the root [MIT license](../../LICENSE).
