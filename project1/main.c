/*
 * main.c
 *
 *  Created on: April 23, 2025
 *      Author: Vikram Thirumaran
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <stdbool.h>
 #include <unistd.h>
 #include <time.h>
 
 #include "string_parser.h"
 #include "command.h"
 
 #define _GNU_SOURCE
 
 FILE *input_stream = NULL;
 
 int main(int argc, char const *argv[]) {
     // Buffer to hold each line of input
     size_t buffer_len = 128;
     char *input_line = malloc(buffer_len);
 
     bool reading_from_file = false;
 
     // Determine input source: stdin or file
     if (argc == 1) {
         // Interactive mode
         write(STDOUT_FILENO, ">>> ", 4);
         input_stream = stdin;
     } else if (argc == 3 && strcmp(argv[1], "-f") == 0) {
         // File mode
         input_stream = fopen(argv[2], "r");
         if (input_stream == NULL) {
             printf("Error: File does not exist\n");
             return -1;
         }
         freopen("output.txt", "w", stdout);
         reading_from_file = true;
     } else {
         // Invalid usage
         printf("Error: Invalid input\n");
         return -1;
     }
 
     command_line semicolon_tokens;
     command_line space_tokens;
 
     // Read input line by line
     while (getline(&input_line, &buffer_len, input_stream) != -1) {
         // Exit command in interactive mode
         if (strcmp(input_line, "exit\n") == 0) {
             break;
         }
 
         // Tokenize by ';' to get individual commands
         semicolon_tokens = str_filler(input_line, ";");
 
         for (int i = 0; i < semicolon_tokens.num_token; i++) {
             // Tokenize each command by spaces
             space_tokens = str_filler(semicolon_tokens.command_list[i], " ");
 
             // Only check the first token, as commands are single-word identifiers
             char *command = space_tokens.command_list[0];
 
             // Command dispatcher with argument count validation
             if (strcmp(command, "ls") == 0) {
                 if (space_tokens.num_token == 1) {
                     listDir();
                 } else {
                     printf("Error: Unsupported parameters for command: ls\n");
                 }
             } else if (strcmp(command, "pwd") == 0) {
                 if (space_tokens.num_token == 1) {
                     showCurrentDir();
                 } else {
                     printf("Error: Unsupported parameters for command: pwd\n");
                 }
             } else if (strcmp(command, "mkdir") == 0) {
                 if (space_tokens.num_token == 2) {
                     makeDir(space_tokens.command_list[1]);
                 } else {
                     printf("Error: Unsupported parameters for command: mkdir\n");
                 }
             } else if (strcmp(command, "cd") == 0) {
                 if (space_tokens.num_token == 2) {
                     changeDir(space_tokens.command_list[1]);
                 } else {
                     printf("Error: Unsupported parameters for command: cd\n");
                 }
             } else if (strcmp(command, "cp") == 0) {
                 if (space_tokens.num_token == 3) {
                     copyFile(space_tokens.command_list[1], space_tokens.command_list[2]);
                 } else {
                     printf("Error: Unsupported parameters for command: cp\n");
                 }
             } else if (strcmp(command, "mv") == 0) {
                 if (space_tokens.num_token == 3) {
                     moveFile(space_tokens.command_list[1], space_tokens.command_list[2]);
                 } else {
                     printf("Error: Unsupported parameters for command: mv\n");
                 }
             } else if (strcmp(command, "rm") == 0) {
                 if (space_tokens.num_token == 2) {
                     deleteFile(space_tokens.command_list[1]);
                 } else {
                     printf("Error: Unsupported parameters for command: rm\n");
                 }
             } else if (strcmp(command, "cat") == 0) {
                 if (space_tokens.num_token == 2) {
                     displayFile(space_tokens.command_list[1]);
                     printf("\n");
                 } else {
                     printf("Error: Unsupported parameters for command: cat\n");
                 }
             } else {
                 printf("Error: Invalid command\n");
             }
 
             // Free tokens from the space-separated parsing
             free_command_line(&space_tokens);
             memset(&space_tokens, 0, sizeof(command_line));
         }
 
         // Prompt for next command if in interactive mode
         if (input_stream == stdin) {
             write(STDOUT_FILENO, ">>> ", 4);
         }
 
         // Free tokens from the semicolon-separated parsing
         free_command_line(&semicolon_tokens);
         memset(&semicolon_tokens, 0, sizeof(command_line));
     }
 
     // Cleanup: close file stream if necessary and free line buffer
     if (reading_from_file) {
         fclose(input_stream);
     }
 
     free(input_line);
     return 0;
 }
 