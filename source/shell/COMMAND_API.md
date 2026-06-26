# ESP32-C3 Micro Linux Shell Command API

The shell command layer can be called by the interactive shell, BASIC, a monitor,
or another runtime. The API lives in `shell.h`.

## Entry Points

Use command-line execution when another program wants shell-style system calls:

```c
shell_status_t status = shell_exec_line("LS /basic", &io);
```

Use enum execution when the caller already parsed the command:

```c
shell_status_t status = shell_exec_command(SHELL_COMMAND_LS, "/basic", &io);
```

Translate a command name to an enum:

```c
shell_command_t command;
if (shell_command_from_name("LS", &command) == SHELL_STATUS_OK) {
    shell_exec_command(command, "/", &io);
}
```

Convert status codes to stable names:

```c
const char *name = shell_status_name(status);
```

Reset shell current-directory state:

```c
shell_reset();
```

Register an optional external command handler:

```c
shell_register_external_exec(bin_exec_line);
```

The root firmware uses this hook to expose `/bin` services such as
`/bin/hardware`. The standalone shell does not register this hook.

## Output And Input

Callers provide an optional `shell_exec_io_t`:

```c
typedef struct {
    shell_write_fn write;
    shell_read_line_fn read_line;
    void *ctx;
    uint32_t flags;
} shell_exec_io_t;
```

If `write` is set, shell output is sent to that callback. This lets BASIC capture
`LS`, `PWD`, `CAT`, and error text. If `write` is `NULL`, output goes to the
default device input backend.

If `read_line` is set and interactive commands are allowed, the shell can ask
for confirmation. Program callers normally leave `read_line` as `NULL` and keep
interactive/destructive flags disabled.

`DF` reports workspace capacity using Unix-style 1K-block columns. `RECV` and
`SEND` are interactive terminal transfer commands. They use the input port's raw
byte API and are rejected unless interactive execution is allowed. Program
callers should use storage or higher-level services instead of invoking YMODEM
transfer through the command API.

`RUN /bin/name.com [args...]` loads only C3COM-format executables. It passes
whitespace-separated argv plus stdin/stdout/stderr callbacks to the executable
through `source/shell/c3_com.h`. On ESP32-C3, this requires executable heap
availability; the project disables ESP-IDF memory protection for native C3COM
execution. First-slice C3COM images must be position-independent because the
loader validates and copies a flat code blob but does not apply relocations.

## Flags

```c
SHELL_EXEC_ALLOW_INTERACTIVE
SHELL_EXEC_ALLOW_DESTRUCTIVE
```

`RENEW` requires both flags. Without them it returns
`SHELL_STATUS_NOT_ALLOWED` and prints `Not allowed`.

This prevents BASIC or other programs from accidentally formatting
`workspace_fs`.

## Status Codes

```text
SHELL_STATUS_OK
SHELL_STATUS_EMPTY
SHELL_STATUS_UNKNOWN_COMMAND
SHELL_STATUS_BAD_INPUT
SHELL_STATUS_BAD_PATH
SHELL_STATUS_NOT_FOUND
SHELL_STATUS_EXISTS
SHELL_STATUS_IS_DIRECTORY
SHELL_STATUS_NOT_DIRECTORY
SHELL_STATUS_IO_ERROR
SHELL_STATUS_CANCELLED
SHELL_STATUS_NOT_ALLOWED
```

The status is a system-call result. Text output is still produced for the user or
caller-provided output sink.

## External `/bin` Services

`shell_exec_line()` first tries the built-in shell command set. If a command is
not built in and an external handler was registered with
`shell_register_external_exec()`, the shell passes the original input line to
that handler.

Current root firmware handler:

```text
/bin/hardware ...
```

External services are not shell built-ins. They are intentionally absent from
`shell_command_t`, `shell_command_from_name()`, and standalone shell `HELP`.

## BASIC-Style Example

```c
typedef struct {
    char *buf;
    size_t len;
    size_t used;
} capture_t;

static void capture_write(void *ctx, const void *data, size_t len)
{
    capture_t *cap = (capture_t *)ctx;
    size_t room = cap->len - cap->used;
    if (room == 0) {
        return;
    }
    if (len > room - 1) {
        len = room - 1;
    }
    memcpy(cap->buf + cap->used, data, len);
    cap->used += len;
    cap->buf[cap->used] = '\0';
}

char output[256] = {0};
capture_t capture = {
    .buf = output,
    .len = sizeof(output),
};
shell_exec_io_t io = {
    .write = capture_write,
    .ctx = &capture,
};

shell_status_t st = shell_exec_line("PWD", &io);
```

## Commands Exposed Through API

All current shell commands are available:

```text
HELP
DF
PWD
LS [path]
CD <path>
MKDIR <path>
RMDIR <dir>
CAT <file>
WRITE [-F] <file> <text>
RM <file>
RM -R <path>
CP <src> <dst>
MV <src> <dst>
RECV [-F] <path>
SEND <path>
RUN /bin/name.com [args...]
RENEW
```

Legacy aliases and BASIC commands remain unavailable through the shell API:

```text
DIR
COPY
MOVE
DELETE
RENAME
LOAD
SAVE
NEW
LIST
RUN
PRINT
```

Hardware services remain external `/bin` commands, not enum shell commands.
