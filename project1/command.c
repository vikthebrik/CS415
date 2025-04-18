#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "command.h"

#define _POSIX_C_SOURCE 200809L
#define BUFFER_SIZE 1024
#define PATH_MAX 4096  // Fallback value if not defined

void write_message(const char *msg) {
    write(STDOUT_FILENO, msg, strlen(msg));
}

// ls
void listDir() {
    DIR *dir = opendir(".");
    if (!dir) {
        write_message("Error: Could not open current directory.\n");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        write_message(entry->d_name);
        write_message(" ");
    }
    write_message("\n");
    closedir(dir);
}

// pwd
void showCurrentDir() {
    char cwd[PATH_MAX];
    if (getcwd(cwd, sizeof(cwd)) != NULL) {
        write(STDOUT_FILENO, cwd, strlen(cwd));
        write(STDOUT_FILENO, "\n", 1);
    } else {
        write_message("Error: Unable to get current directory.\n");
    }
}

// mkdir
void makeDir(char *dirName) {
    if (mkdir(dirName, 0755) == -1) {
        if (errno == EEXIST)
            write_message("Directory already exists!\n");
        else
            write_message("Error: Unable to create directory.\n");
    }
}

// cd
void changeDir(char *dirName) {
    if (chdir(dirName) == -1) {
        write_message("Error: Unable to change directory.\n");
    }
}

// cp
void copyFile(char *sourcePath, char *destinationPath) {
    int src_fd = open(sourcePath, O_RDONLY);
    if (src_fd < 0) {
        write_message("Error: Source file not found.\n");
        return;
    }

    int dst_fd = open(destinationPath, O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (dst_fd < 0) {
        write_message("Error: Cannot open destination file.\n");
        close(src_fd);
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = read(src_fd, buffer, BUFFER_SIZE)) > 0) {
        write(dst_fd, buffer, bytesRead);
    }

    close(src_fd);
    close(dst_fd);
}

// mv
void moveFile(char *sourcePath, char *destinationPath) {
    copyFile(sourcePath, destinationPath);
    if (remove(sourcePath) != 0) {
        write_message("Error: Failed to remove source after moving.\n");
    }
}

// rm
void deleteFile(char *filename) {
    if (remove(filename) != 0) {
        write_message("Error: Could not delete file.\n");
    }
}

// cat
void displayFile(char *filename) {
    int fd = open(filename, O_RDONLY);
    if (fd < 0) {
        write_message("Error: Cannot open file.\n");
        return;
    }

    char buffer[BUFFER_SIZE];
    ssize_t bytesRead;
    while ((bytesRead = read(fd, buffer, BUFFER_SIZE)) > 0) {
        write(STDOUT_FILENO, buffer, bytesRead);
    }

    close(fd);
}