#include<stdio.h>
#include <sys/types.h>
#include<stdlib.h>
#include<unistd.h>
#include<sys/wait.h>

void script_print (pid_t* pid_ary, int size);

int main(int argc,char*argv[])
{

	/*
	*	TODO
	*	#1	declare child process pool
	*	#2 	spawn n new processes
	*		first create the argument needed for the processes
	*		for example "./iobound -seconds 10"
	*	#3	call script_print
	*	#4	wait for children processes to finish
	*	#5	free any dynamic memory
	*/

	if (argc != 2) {
        printf("Usage: %s <num_processes>\n", argv[0]);
        exit(1);
    }

    int n = atoi(argv[1]);
    pid_t* pid_array = malloc(n * sizeof(pid_t));
    if (pid_array == NULL) {
        perror("malloc");
        exit(1);
    }

    for (int i = 0; i < n; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        } else if (pid == 0) {
            // Child process
            char* args[] = {"./iobound", "-seconds", "5", NULL};
            if (execvp(args[0], args) == -1) {
                perror("execvp failed");
                exit(1);
            }
        } else {
            // Parent stores child PID
            pid_array[i] = pid;
        }
    }

    // Create monitoring script and launch it
    script_print(pid_array, n);

    // Wait for all child processes to finish
    for (int i = 0; i < n; i++) {
        waitpid(pid_array[i], NULL, 0);
    }

    free(pid_array);
    return 0;
}


void script_print (pid_t* pid_ary, int size)
{
	FILE* fout;
	fout = fopen ("top_script.sh", "w");
	fprintf(fout, "#!/bin/bash\ntop");
	for (int i = 0; i < size; i++)
	{
		fprintf(fout, " -p %d", (int)(pid_ary[i]));
	}
	fprintf(fout, "\n");
	fclose (fout);

	char* top_arg[] = {"gnome-terminal", "--", "bash", "top_script.sh", NULL};
	pid_t top_pid;

	top_pid = fork();
	{
		if (top_pid == 0)
		{
			if(execvp(top_arg[0], top_arg) == -1)
			{
				perror ("top command: ");
			}
			exit(0);
		}
	}
}


