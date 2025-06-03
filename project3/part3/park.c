// File: part3/park.c - Monitor System using Pipe IPC
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <pthread.h>
#include <string.h>
#include <time.h>

#define BUFFER_SIZE 256

void* monitor_thread(void* arg) {
    char buffer[BUFFER_SIZE];
    FILE* pipe = fdopen(STDIN_FILENO, "r");
    if (!pipe) {
        perror("fdopen");
        pthread_exit(NULL);
    }
    while (fgets(buffer, sizeof(buffer), pipe)) {
        printf("[Monitor] %s", buffer);
        fflush(stdout);
    }
    fclose(pipe);
    return NULL;
}

void* simulate_thread(void* arg) {
    for (int i = 0; i < 10; i++) {
        char msg[BUFFER_SIZE];
        time_t now = time(NULL);
        struct tm* tm_info = localtime(&now);
        char time_str[16];
        strftime(time_str, sizeof(time_str), "%H:%M:%S", tm_info);

        snprintf(msg, sizeof(msg), "[Time: %s] Ride Queue Length: %d\n", time_str, rand() % 5);
        write(STDOUT_FILENO, msg, strlen(msg));
        sleep(2);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    pthread_t monitor, simulate;

    // If used as a standalone, launch simulator internally (test mode)
    if (isatty(STDIN_FILENO)) {
        int pipefd[2];
        pipe(pipefd);

        if (fork() == 0) {
            dup2(pipefd[1], STDOUT_FILENO);
            close(pipefd[0]); close(pipefd[1]);
            simulate_thread(NULL);
            exit(0);
        }

        dup2(pipefd[0], STDIN_FILENO);
        close(pipefd[1]); close(pipefd[0]);
    }

    pthread_create(&monitor, NULL, monitor_thread, NULL);
    pthread_join(monitor, NULL);
    return 0;
}
