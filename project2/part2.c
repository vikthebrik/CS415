#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "string_parser.h"
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>

#define _GNU_SOURCE
FILE *stream = NULL;
int count_lines(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Unable to open file");
        return -1;
    }

    int lines = 0;
    char buffer[1024]; // Adjust buffer size as needed

    while (fgets(buffer, sizeof(buffer), file) != NULL) {
        lines++;
    }
    fclose(file);
    return lines;
}

void waitForSignal(int signum) {
    sigset_t sigset;
    int sig;
    sigemptyset(&sigset);
    sigaddset(&sigset, signum);
    sigprocmask(SIG_BLOCK, &sigset, NULL);
    sigwait(&sigset, &sig);
}

void sendSignalToAll(pid_t *p_array, int n, int signal) {
	for (int i = 0; i < n; i++) {
		//printf("sending!\n");
        kill(p_array[i], signal);
    }
}

int main(int argc, char const *argv[])
{
	//checking for command line argument
	size_t len = 128;
    char* line_buf = malloc(len);
	
	bool ifFile = false;

	if (argc == 1) {
        write(STDOUT_FILENO, ">>> ", 4);
		stream = stdin;
    }
	else if (argc == 3 && strcmp(argv[1], "-f") == 0) {
		stream = fopen(argv[2], "r");
		if(stream == NULL){
			printf("Error file does not exist\n");
			return -1;
		}
		//freopen("output.txt", "w", stdout);
		ifFile = true;
    }else{
		printf("Error error invalid input\n");
		return -1;
	}
	
	command_line large_token_buffer;
	command_line small_token_buffer;
	int commands = count_lines(argv[2]);
	int line_num = 0;
	int numChildren = 0;

	//loop until the file is over
	pid_t *pid_array = (pid_t *) malloc(sizeof(*pid_array)*commands);
	while (getline(&line_buf, &len, stream) != -1)
	{		
		
		//printf("the line: %s\n", line_buf);
		large_token_buffer = str_filler (line_buf, ";");
		//printf(" large token %d", large_token_buffer.num_token);
		//iterate through each large token

			//tokenize large buffer
			//smaller token is seperated by " "(space bar)
			small_token_buffer = str_filler(large_token_buffer.command_list[0], " ");
	
			pid_array[line_num] = fork();
			//if(pid_array[line_num] > 1)printf("pid_array%d\n", pid_array[line_num]);
			//if(pid_array[line_num] == 0)printf("pid_arrayChild%d\n", pid_array[line_num]);
			if (pid_array[line_num] < 0)
			{
				printf("Error forking!!\n");
			}
			if (pid_array[line_num] == 0)
			{
				waitForSignal(SIGUSR1);
				//printf("process running!\n");
				if (execvp(small_token_buffer.command_list[0], small_token_buffer.command_list) == -1)
				{
					printf("Error, invalid command.\n");
				}
			//
            free(pid_array);
            free(line_buf);
            free_command_line (&small_token_buffer);
		    memset (&small_token_buffer, 0, 0);
            free_command_line (&large_token_buffer);
		    memset (&large_token_buffer, 0, 0);
            fclose(stream);
			exit(-1);
			}
			numChildren++;
			line_num++;
		//free smaller tokens and reset variable
        free_command_line (&small_token_buffer);
		memset (&small_token_buffer, 0, 0);
		free_command_line(&large_token_buffer);
		memset (&large_token_buffer, 0, 0);
	}

	sleep(1);
	sendSignalToAll(pid_array, numChildren, SIGUSR1);
	sleep(1);
	sendSignalToAll(pid_array, numChildren, SIGSTOP);
	sleep(1);
	sendSignalToAll(pid_array, numChildren, SIGCONT);
	

	int status;
	pid_t p;
    while (numChildren > 0) {
        p = wait(&status); 
		if(p == -1){
			if(errno == EINTR){
				continue;}//specifically handle case where wait is interrupted by singal
		}
		if(WIFEXITED(status) > 0){
			//printf("Process terminated by signal %d", p);
			numChildren--;
		}
    }

	free(pid_array);
	if(ifFile){fclose(stream);}
	//free line buffer
	free (line_buf);
}
