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

int count_token (char* buf, const char* delim)
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
//|| buf[0] == '\n' || buf[0] == '\0'
	if(buf == NULL){return 0;}
	int count = 0;
    char* tmp = strdup(buf); 
	char* save_ptr;
    char* token = strtok_r(tmp, delim, &save_ptr);
	while (token){
		count++;
		token = strtok_r(NULL, delim, &save_ptr);
	}
	free(tmp);
	return count;
}

command_line str_filler (char* buf, const char* delim)
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
	int x = 0;
	char* save_ptr;
	
	for(int i = 0; i<strlen(buf); i++){
		if(buf[i] == '\n'){
			buf[i] = '\0';
		}
	}
	command_line fill;
	if (buf == NULL) {
		return fill;  // Return an empty command_line if input is NULL or empty
	}
	fill.num_token = count_token(buf, delim);
	int i = 0;


	fill.command_list = (char**) malloc(sizeof(char*) * (fill.num_token+1));
	fill.command_list[fill.num_token] = NULL;
	
	char* save_ptr2;
    char* token = strtok_r(buf, delim, &save_ptr2);
	
	//char* temp = &save_ptr;

	while(token != NULL && i <fill.num_token){
		fill.command_list[i] = strdup(token);
		token = strtok_r(NULL, delim, &save_ptr2);
        i++;
	}
	return fill;
}


void free_command_line(command_line* command)
{
	//TODO：
	/*
	*	#1.	free the array base num_token
	*/
	for(int i = 0; i<command->num_token; i++){
		free(command->command_list[i]);
	}
	free(command->command_list);

}
