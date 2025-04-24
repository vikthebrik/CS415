
# Project 1: Pseudo Shell Implementation

This project is part of the CS415 course and involves creating a simplified shell program that can parse and execute user commands.

## Overview

The objective of this project is to develop a pseudo shell that can interpret and execute user commands. The shell should be capable of handling basic command execution, input/output redirection, and piping between commands.

## Directory Structure

- `main.c`
- `command.c`
- `command.h`
- `string_parser.c`
- `string_parser.h`
- `Makefile`
- `input.txt`
- `example-output.txt`
- `test_script.sh`
- `Report_Collection_Template.docx`
- `project-1-description-fall-2024 (1) copy 2.pdf`
- `pseudo-shell-arm-based`
- `pseudo-shell-intel-based`

## Compilation

To compile the project, navigate to the `project1` directory and run:

```bash
make
```

This will generate the `pseudo-shell` executable.

## Usage

To run the pseudo shell, execute:

```bash
./pseudo-shell
```

The shell will prompt for user input. Enter commands as you would in a typical shell.

### Example

```bash
$ ./pseudo-shell
pseudo-shell> ls -l
total 12
-rw-r--r-- 1 user user  123 Apr 24 16:58 file1.txt
-rw-r--r-- 1 user user  456 Apr 24 16:58 file2.txt
pseudo-shell> exit
```

## Features

- Execution of standard commands (e.g., `ls`, `pwd`)
- Input/output redirection using `>` and `<`
- Piping between commands using `|`
- Basic error handling for invalid commands

## Testing

A test script `test_script.sh` is provided to automate testing of the shell's functionalities. To run the tests:

```bash
./test_script.sh
```

The script will execute a series of commands and compare the output against expected results.

## Documentation

- `project-1-description-fall-2024 (1) copy 2.pdf`: Detailed project description and requirements.
- `Report_Collection_Template.docx`: Template for the project report submission.

## Notes

- Ensure that the shell is executed in a compatible environment (e.g., Linux or macOS terminal).
- The provided `pseudo-shell-arm-based` and `pseudo-shell-intel-based` executables are precompiled binaries for ARM and Intel architectures, respectively.
- Modify and extend the shell functionalities as per the project requirements outlined in the documentation.
