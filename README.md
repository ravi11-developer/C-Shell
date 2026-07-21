# C-Shell

Unix-like shell written in C for Linux. The project implements command tokenization, syntax validation, execution of external programs, several built-in commands, pipelines, redirections, command chaining, and foreground/background job control.

## Project Description

This repository contains a custom shell that reads a command line from standard input, tokenizes it, validates the syntax, converts it into a linked command sequence, and then executes each command with support for pipes, redirection, and job control. The shell also implements a small set of built-ins for directory navigation, file listing, command history, process signaling, and background job inspection.

## Features

- Command parsing for `;`, `|`, `&`, `<`, `>`, and `>>`
- External command execution through `fork` and `execvp`
- Foreground and background process handling
- Process group management and terminal handoff for foreground jobs
- Signal handling for `SIGINT` and `SIGTSTP`
- Built-in commands: `hop`, `reveal`, `log`, `ping`, `activities`, `fg`, and `bg`
- Rolling command history persisted in `log.txt`
- Background job tracking with monotonic job numbers
- Job status inspection through `/proc/<pid>/stat`

## Architecture

The code is organized as a simple shell pipeline:

1. `src/tokenize.c` splits raw input into tokens.
2. `src/A3.c` validates the token stream against a small grammar.
3. `src/ast.c` converts tokens into a linked list of `Command` nodes.
4. `src/shell.c` executes commands, handles built-ins, manages jobs, and restores terminal state.
5. `src/B1.c`, `src/B2.c`, and `src/ping.c` implement the built-in commands.

This is not a tree-shaped AST in the strict compiler sense. The `Command` structure is a linked command chain that the shell walks from left to right.

## Tech Stack

- Languages: C
- Libraries: standard C library, POSIX/Linux system calls
- Frameworks: none
- Databases: none
- Tools: `gcc`, `make`, Linux `/proc`, terminal control APIs

## Problem Statement

The project solves the problem of implementing a usable interactive shell rather than just a command runner. It needs to interpret user input, support shell operators, preserve responsive control of the terminal, and keep the shell alive while foreground jobs are interrupted or stopped.

## System Design

The shell follows a linear input-to-execution flow. A line of text is read, tokenized, validated, and transformed into `Command` nodes. `shell.c` then decides whether each command should run in the parent process as a built-in or in a child process as an external command. Foreground jobs are given terminal control with `tcsetpgrp`, while background jobs are detached into their own process group and tracked in an in-memory job table.

Redirection is applied by duplicating file descriptors before `execvp` or before running built-ins that are executed in a child context. History is stored in `log.txt`, and job state is refreshed using `waitpid(..., WNOHANG)` and `/proc` status inspection.

## Important Modules

### `src/shell.c`
Purpose: Main shell loop, execution engine, signal handlers, history handling, and job control.
Files: `src/shell.c`, `include/shell.h`
Key Logic: Reads input, updates history, validates syntax, dispatches built-ins, forks children for external commands, manages pipes and redirections, and tracks foreground/background jobs.

### `src/tokenize.c`
Purpose: Tokenizes shell input into discrete symbols and command fragments.
Files: `src/tokenize.c`, `include/tokenize.h`
Key Logic: Splits on whitespace, recognizes `;`, `|`, and `&`, and preserves combined tokens such as `>file` or `<input` for later parsing.

### `src/A3.c`
Purpose: Grammar validation for command sequences.
Files: `src/A3.c`, `include/A3.h`
Key Logic: Implements a recursive-descent style validator for atomic commands, pipes, input/output redirection, and command chaining.

### `src/ast.c`
Purpose: Builds the command chain used by the shell executor.
Files: `src/ast.c`, `include/ast.h`
Key Logic: Creates `Command` nodes, duplicates token strings, and links commands together using `next`.

### `src/B1.c`
Purpose: Implements the `hop` built-in.
Files: `src/B1.c`, `include/B1.h`
Key Logic: Changes directories, supports `~`, `..`, `-`, and explicit paths, and tracks the previous working directory.

### `src/B2.c`
Purpose: Implements the `reveal` built-in.
Files: `src/B2.c`, `include/B2.h`
Key Logic: Lists directory entries, supports `-a` and `-l` style flags, sorts results, and handles path aliases such as `~`, `.`, `..`, and `-`.

### `src/ping.c`
Purpose: Implements the `ping` built-in.
Files: `src/ping.c`
Key Logic: Validates numeric arguments and sends a signal to a process using `kill`.

### `src/A1.c`
Purpose: Builds the shell prompt string.
Files: `src/A1.c`, `include/A1.h`
Key Logic: Formats `username@hostname:cwd` and shortens the home directory to `~` when applicable.

## Algorithms / Technical Concepts Used

- Recursive-descent grammar validation
- Linked-list command sequencing
- Process creation with `fork`/`execvp`
- File descriptor duplication with `dup`/`dup2`
- Pipes with `pipe`
- Foreground/background process groups with `setpgid`
- Terminal handoff with `tcsetpgrp`
- Signal forwarding and handling with `signal`, `kill`, and `waitpid`
- Directory traversal with `opendir`, `readdir`, and `closedir`
- Sorting with `qsort`
- `/proc`-based process state inspection

## Challenges Solved

- Keeping the shell responsive while foreground jobs run, stop, or exit
- Forwarding interactive signals to the correct process group without killing the shell itself
- Supporting built-ins both in the parent process and in pipeline contexts
- Handling command history without introducing duplicate consecutive entries
- Supporting both spaced and compact redirection syntax such as `> file` and `>file`
- Tracking background jobs and pruning stale entries as processes exit

## Unique Technical Aspects

- Uses terminal process-group control rather than only `waitpid`-based job management
- Treats stopped jobs as first-class objects with `fg` and `bg` support
- Derives job state from `/proc/<pid>/stat` instead of relying only on cached state
- Stores command history in a flat file and supports replay with `log execute <n>`
- Preserves the shell’s own terminal state when running pipelines and redirections

## README Quality Review

The existing README is a good start, but it is not fully accurate and leaves out several important implementation details.

Strengths:

- It names the major user-facing commands
- It explains the broad intent of the project
- It includes example usage and a build command

Missing or weak areas:

- It overstates the parser architecture by describing a true AST, while the implementation uses a linked command chain
- It does not explain the actual build requirements beyond `make`
- It does not mention that the project targets Linux-specific behavior such as `/proc` and terminal control
- It does not document the history file `log.txt`
- It does not state that there is no database or external service dependency
- It lacks a license section
- It lacks screenshots or UI guidance
- It does not clearly describe the relationship between tokenization, validation, and execution

## Installation

```bash
make
```

This builds `shell.out` in the repository root.

## Usage

```bash
./shell.out
```

Example commands:

```bash
hop ~
reveal -al .
sleep 30 &
activities
fg 1
bg 1
ping 12345 9
log
log execute 3
cat input.txt | grep error > output.txt
```

## Configuration

No separate configuration file is required. The shell uses the current working directory, the initial home directory, and the `log.txt` history file created in the repository root during execution.

## Project Structure

```
├── Makefile
├── README.md
├── include/
│   ├── A1.h
│   ├── A3.h
│   ├── ast.h
│   ├── B1.h
│   ├── B2.h
│   ├── shell.h
│   └── tokenize.h
└── src/
	├── A1.c
	├── A3.c
	├── ast.c
	├── B1.c
	├── B2.c
	├── ping.c
	├── shell.c
	└── tokenize.c
```


## Future Improvements

- Add wildcard expansion and quoted string handling
- Free allocated token and command memory consistently
- Support more POSIX shell syntax such as subshells and logical operators
- Improve built-in behavior inside pipelines where appropriate
- Add automated tests for parser, job control, and history behavior


