# Wisconsin Shell (wsh)

This document outlines how to build, run, and utilize wsh, as well as its capabilities.

## Objectives
- Familiarize with the Linux programming environment.
- Learn about process creation, destruction, and management.
- Gain exposure to shell functionalities.

## Overview
wsh is a command line interpreter that operates in a basic manner: it waits for a command, executes it as a child process, and waits for the process to finish before prompting for more input. It supports both interactive and batch modes.

### Interactive Mode
Run wsh without arguments to enter interactive mode, where commands can be directly typed into the shell.
```bash
prompt> ./wsh
wsh>
```

### Batch Mode
wsh also supports batch mode, taking commands from a file.
```bash
prompt> ./wsh script.wsh
```
Note: Batch mode does not display the prompt.

## Program Specifications

### Basic Shell Operations
- **Loop Operation**: wsh runs in a loop, executing commands until `exit` is typed.
- **Input**: Uses `getline()` for reading commands, supporting arbitrarily long inputs.
- **Command Parsing**: Utilizes `strsep()` to parse the input into commands and arguments.
- **Execution**: Employs `fork()`, `execvp()`, and `wait()/waitpid()` for command execution. Does not use `system()` calls.

### Pipes
- Supports pipes (`|`), allowing output of one program to be the input of another.
- Example: `cat f.txt | gzip -c | gunzip -c | tail -n 10`

### Environment and Shell Variables
- **Environment Variables**: Inherited by child processes, managed with `getenv` and `setenv`.
- **Shell Variables**: Managed with `local` for session-specific variables, not inherited by child processes.
- **Display Variables**: Use `vars` to display shell variables, and `env` for environment variables.

### Path
- wsh uses the `PATH` environment variable to find executables for commands.

### History
- Maintains a history of the last five commands. Use `history` to view and `history set <n>` to configure the capacity.

## Building and Running wsh

1. **Compiling wsh**:
   Ensure you have a C compiler and standard build utilities installed. Compile `wsh` with:
   ```bash
   gcc -o wsh wsh.c
   ```
2. **Running wsh**:
   - For interactive mode: `./wsh`
   - For batch mode: `./wsh script.wsh`

## Features and Commands

- **Executing Programs**: Type the command and arguments, e.g., `ls -la /tmp`.
- **Pipes**: Chain commands with `|`, e.g., `grep foo file.txt | less`.
- **Environment Variables**: Use `export VAR=value` to set environment variables.
- **Shell Variables**: Use `local VAR=value` to set shell-specific variables.
- **Variable Display**: Use `vars` to display shell variables, `env` to display environment variables.
- **History**: Use `history` to view command history, and `history set <n>` to adjust history size.
