/*
 * string_parser.c
 *
 *  Created on: Nov 25, 2020
 *      Author: gguan, Monil
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"

#define _GUN_SOURCE
#define MAX_COMMAND_LINE_ARGUMENTS 10

int count_token(char* buf, const char* delim)
{
    //TODO：
	/*
	*	#1.	Check for NULL string
	*	#2.	iterate through string counting tokens
	*		Cases to watchout for
	*			a.	string start with delimeter
	*			b. 	string end with delimeter
	*			c.	account NULL for the last token
	*	#3. return the number of token (note not number of delimeter)
	*/
	if (!buf || !delim) return -1;  // Check for null pointers

    int count = 0;

    // Copy input buffer because strtok_r modifies it
    char* temp = malloc(strlen(buf) + 1);
    if (!temp) {
        perror("malloc failed");
        exit(1);
    }
    strcpy(temp, buf);  // Safe copy

    char* saveptr;
    char* token = strtok_r(temp, delim, &saveptr);  // Start tokenizing

    while (token != NULL) {
        // Skip pure whitespace or newline tokens
        while (*token == ' ' || *token == '\t' || *token == '\n') token++;

        if (*token != '\0') {
            size_t len = strlen(token);
            // Trim trailing whitespace
            while (len > 0 && (token[len - 1] == ' ' || token[len - 1] == '\t' || token[len - 1] == '\n')) {
                token[--len] = '\0';
            }

            if (len > 0) count++;  // Only count if actual content remains
        }

        token = strtok_r(NULL, delim, &saveptr);
    }

    free(temp);  // Clean up
    return count;
}


command_line str_filler(char* buf, const char* delim)
{
    //TODO：
	/*
	*	#1.	create command_line variable to be filled and returned
	*	#2.	count the number of tokens with count_token function, set num_token. 
    *           one can use strtok_r to remove the \n at the end of the line.
	*	#3. malloc memory for token array inside command_line variable
	*			based on the number of tokens.
	*	#4.	use function strtok_r to find out the tokens 
    *   #5. malloc each index of the array with the length of tokens,
	*			fill command_list array with tokens, and fill last spot with NULL.
	*	#6. return the variable.
	*/
	command_line cl;
    cl.num_token = 0;

    char* temp_tokens[MAX_COMMAND_LINE_ARGUMENTS];
    int count = 0;

    char* saveptr;
    char* token = strtok_r(buf, delim, &saveptr);

    while (token != NULL && count < MAX_COMMAND_LINE_ARGUMENTS) {
        // Trim leading and trailing whitespace manually
        while (*token == ' ' || *token == '\t' || *token == '\n') token++;

        // Skip if token is now empty after trimming
        if (*token != '\0') {
            size_t len = strlen(token);

            // Also trim trailing whitespace
            while (len > 0 && (token[len - 1] == ' ' || token[len - 1] == '\t' || token[len - 1] == '\n')) {
                token[--len] = '\0';
            }

            if (len > 0) {
                temp_tokens[count] = malloc(len + 1);
                if (!temp_tokens[count]) {
                    perror("malloc failed");
                    exit(1);
                }
                strcpy(temp_tokens[count], token);
                count++;
            }
        }

        token = strtok_r(NULL, delim, &saveptr);
    }

    cl.command_list = malloc((count + 1) * sizeof(char*));
    if (!cl.command_list) {
        perror("malloc failed");
        exit(1);
    }

    for (int i = 0; i < count; i++) {
        cl.command_list[i] = temp_tokens[i];
    }

    cl.command_list[count] = NULL;
    cl.num_token = count;

    return cl;
}


void free_command_line(command_line* command)
{
    //TODO：
	/*
	*	#1.	free the array base num_token
	*/
	if (!command || !command->command_list) return;

    for (int i = 0; i < command->num_token; i++) {
        free(command->command_list[i]);          // Free each string
        command->command_list[i] = NULL;         // Avoid dangling pointer
    }

    free(command->command_list);  // Free the pointer array itself
    command->command_list = NULL;
    command->num_token = 0;
}

