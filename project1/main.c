#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>

#include "string_parser.h"
#include "command.h"

#define MAX_LINE 2048
#define PROMPT ">>> "

// Trim leading and trailing whitespace
char *trim_whitespace(char *str) {
    while (*str == ' ' || *str == '\t') str++;
    char *end = str + strlen(str) - 1;
    while (end > str && (*end == ' ' || *end == '\n' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    return str;
}

// Executes a single command string (not a full line)
void execute_command(char *command_str, int file_mode, int out_fd) {
    command_line *cmd = str_filler(command_str);
    if (cmd == NULL || cmd->num_token == 0) {
        free_command_line(cmd);
        return;
    }

    char **args = cmd->command_list;
    int argc = cmd->num_token;

    // Redirect stdout if in file mode
    int saved_stdout = -1;
    if (file_mode) {
        saved_stdout = dup(STDOUT_FILENO);
        dup2(out_fd, STDOUT_FILENO);
    } else {
        write(STDOUT_FILENO, PROMPT, strlen(PROMPT));
    }

    // Command dispatch
    if (strcmp(args[0], "ls") == 0 && argc == 1) {
        listDir();
    } else if (strcmp(args[0], "pwd") == 0 && argc == 1) {
        showCurrentDir();
    } else if (strcmp(args[0], "mkdir") == 0 && argc == 2) {
        makeDir(args[1]);
    } else if (strcmp(args[0], "cd") == 0 && argc == 2) {
        changeDir(args[1]);
    } else if (strcmp(args[0], "cp") == 0 && argc == 3) {
        copyFile(args[1], args[2]);
    } else if (strcmp(args[0], "mv") == 0 && argc == 3) {
        moveFile(args[1], args[2]);
    } else if (strcmp(args[0], "rm") == 0 && argc == 2) {
        deleteFile(args[1]);
    } else if (strcmp(args[0], "cat") == 0 && argc == 2) {
        displayFile(args[1]);
    } else if (strcmp(args[0], "exit") == 0 && argc == 1) {
        if (file_mode) {
            dup2(saved_stdout, STDOUT_FILENO);
            close(out_fd);
        }
        free_command_line(cmd);
        exit(0);
    } else {
        write(STDOUT_FILENO, "Error! Unrecognized or invalid command.\n", 40);
    }

    if (file_mode) {
        dup2(saved_stdout, STDOUT_FILENO);
        close(saved_stdout);
    }

    free_command_line(cmd);
}

// Main loop
int main(int argc, char *argv[]) {
    char *line = NULL;
    size_t len = 0;
    ssize_t read;

    int file_mode = 0;
    FILE *input_stream = stdin;
    int output_fd = STDOUT_FILENO;

    // File mode setup
    if (argc == 3 && strcmp(argv[1], "-f") == 0) {
        file_mode = 1;
        input_stream = fopen(argv[2], "r");
        if (!input_stream) {
            perror("Error opening input file");
            return 1;
        }

        output_fd = open("output.txt", O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (output_fd < 0) {
            perror("Error creating output.txt");
            fclose(input_stream);
            return 1;
        }
    } else if (argc > 1) {
        write(STDOUT_FILENO, "Usage: ./pseudo-shell [-f <filename>]\n", 38);
        return 1;
    }

    // Shell loop
    while ((read = getline(&line, &len, input_stream)) != -1) {
        // Remove newline and skip empty lines
        if (read == 1) continue;

        // Split by semicolon
        char *saveptr;
        char *command = strtok_r(line, ";", &saveptr);
        while (command) {
            char *trimmed = trim_whitespace(command);
            if (strlen(trimmed) > 0)
                execute_command(trimmed, file_mode, output_fd);
            command = strtok_r(NULL, ";", &saveptr);
        }

        if (!file_mode)
            write(STDOUT_FILENO, "\n", 1);
    }

    if (file_mode) {
        fclose(input_stream);
        close(output_fd);
    }

    free(line);
    return 0;
}
