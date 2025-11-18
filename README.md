# Sigshell

A minimal Unix shell written in C that demonstrates advanced signal handling, process groups, and job control. It implements a robust mechanism for managing foreground processes and signal delivery between the shell and its children.

## Features

### 🛡️ Signal Handling & Job Control

- **SIGINT (Ctrl+C) Protection**: The shell itself catches Ctrl+C and displays a friendly message instead of terminating.
- **Selective Process Protection**: Specific commands (e.g., `sleep`) are hardcoded to run with `SIGINT` disabled, making them immune to Ctrl+C.
- **Native Job Control**: Uses `tcsetpgrp` to properly manage terminal foreground process groups.
- **Process Suspension**: Correctly handles `SIGTSTP` (Ctrl+Z) to suspend processes and return control to the shell.

### 🔧 Shell Capabilities

- Execute external commands with arguments.
- Built-in commands: `cd`, `help`, `exit`.
- Automatic detection of interactive mode.
- proper handling of "zombie" processes via `waitpid`.

## Building the Project

### Prerequisites

- GCC compiler (or any C99-compatible compiler).
- Unix-like operating system (Linux, macOS, BSD).
- POSIX standard libraries.

### Compilation

The code uses POSIX 2008 standards. Compile using:

```bash
gcc -o sigshell sigshell.c