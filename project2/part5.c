#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <signal.h>
#include <errno.h>
#include <time.h>
#include "string_parser.h"

#define _GNU_SOURCE

static FILE *input_stream = NULL;
static pid_t *children;
static int total_processes = 0;
static int current_index = 0;
static int active_children = 0;

int count_lines(const char *filename) {
    FILE *file = fopen(filename, "r");
    if (!file) {
        perror("Failed to open input file");
        return -1;
    }
    int lines = 0;
    char buffer[1024];
    while (fgets(buffer, sizeof(buffer), file)) {
        lines++;
    }
    fclose(file);
    return lines;
}

void print_proc_stats() {
    printf("PID  | utime   stime   total   nice   vmem\n");
    for (int i = 0; i < total_processes; ++i) {
        if (kill(children[i], 0) == -1) continue;

        char stat_path[256], buffer[1024];
        snprintf(stat_path, sizeof(stat_path), "/proc/%d/stat", children[i]);
        FILE *fp = fopen(stat_path, "r");
        if (!fp) continue;

        if (fgets(buffer, sizeof(buffer), fp)) {
            command_line tokens = str_filler(buffer, " ");
            double utime = atof(tokens.command_list[13]) / sysconf(_SC_CLK_TCK);
            double stime = atof(tokens.command_list[14]) / sysconf(_SC_CLK_TCK);
            long nice = strtol(tokens.command_list[18], NULL, 10);
            unsigned long vmem = strtoul(tokens.command_list[22], NULL, 10);
            printf("%5s | %6.2f  %6.2f  %6.2f  %5ld  %lu\n",
                   tokens.command_list[0], utime, stime, utime + stime, nice, vmem);
            free_command_line(&tokens);
        }
        fclose(fp);
    }
}

bool is_io_bound(pid_t pid) {
    char path[256], buffer[1024];
    snprintf(path, sizeof(path), "/proc/%d/io", pid);
    FILE *file = fopen(path, "r");
    if (!file) return false;

    long read_bytes = 0, write_bytes = 0;
    for (int i = 0; i < 2 && fgets(buffer, sizeof(buffer), file); ++i) {
        char *token = strtok(buffer, ":");
        token = strtok(NULL, " ");
        if (i == 0) read_bytes = strtol(token, NULL, 10);
        else write_bytes = strtol(token, NULL, 10);
    }
    fclose(file);
    return read_bytes > write_bytes;
}

void switch_process() {
    print_proc_stats();
    kill(children[current_index], SIGSTOP);

    do {
        current_index = (current_index + 1) % total_processes;
    } while (kill(children[current_index], 0) == -1);

    if (active_children > 1) {
        int time_slice = is_io_bound(children[current_index]) ? 2 : 1;
        kill(children[current_index], SIGCONT);
        alarm(time_slice);
    } else {
        kill(children[current_index], SIGCONT);
    }
}

void setup_scheduler() {
    struct sigaction sa;
    sa.sa_handler = switch_process;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0;
    sigaction(SIGALRM, &sa, NULL);
    alarm(1);
}

int main(int argc, char *argv[]) {
    if (argc == 3 && strcmp(argv[1], "-f") == 0) {
        input_stream = fopen(argv[2], "r");
        if (!input_stream) {
            perror("Failed to open input file");
            return EXIT_FAILURE;
        }
    } else {
        fprintf(stderr, "Usage: %s -f <input_file>\n", argv[0]);
        return EXIT_FAILURE;
    }

    total_processes = count_lines(argv[2]);
    if (total_processes <= 0) return EXIT_FAILURE;

    children = malloc(sizeof(pid_t) * total_processes);
    char *line = NULL;
    size_t len = 0;
    int idx = 0;

    while (getline(&line, &len, input_stream) != -1) {
        command_line full_cmd = str_filler(line, ";");
        command_line args = str_filler(full_cmd.command_list[0], " ");

        pid_t pid = fork();
        if (pid == 0) {
            raise(SIGSTOP);
            execvp(args.command_list[0], args.command_list);
            perror("execvp failed");
            exit(EXIT_FAILURE);
        } else if (pid > 0) {
            children[idx++] = pid;
            active_children++;
        } else {
            perror("fork failed");
        }

        free_command_line(&full_cmd);
        free_command_line(&args);
    }
    free(line);

    current_index = total_processes - 1;
    setup_scheduler();

    int status;
    while (active_children > 0) {
        pid_t finished = wait(&status);
        if (finished == -1) {
            if (errno == EINTR) continue;
            else break;
        }
        active_children--;
    }

    printf("All processes complete.\n");
    free(children);
    fclose(input_stream);
    return EXIT_SUCCESS;
}
