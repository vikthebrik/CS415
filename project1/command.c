/*
 * command.c
 *
 *  Created on: 04/23/2025
 *      Author: Vikram Thirumaran
 *
 *	Purpose: The goal of this file is to implement the commands for the shell. 
 *			 The commands include ls, pwd, mkdir, cd, cp, mv, rm, and cat.
 *
 *
 */
#include <stdio.h>
#include <dirent.h> 
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <libgen.h>
#include <errno.h>
extern FILE* stream;

// ls
void listDir(){
    DIR* dir = opendir(".");
    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL) {
        int len = strlen(entry->d_name);
        write(STDOUT_FILENO, entry->d_name, len);
        write(STDOUT_FILENO, " ", 1);
    }
    write(STDOUT_FILENO, "\n", 1);
    closedir(dir);
    
} 

// pwd
void showCurrentDir(){
    char* buffer;
    size_t size = 1024;
    buffer = malloc(size);
    getcwd(buffer, size);
    write(STDOUT_FILENO, buffer, strlen(buffer));
    write(STDOUT_FILENO, "\n",1);
    free(buffer);
} 

// mkdir
void makeDir(char *dirName){
    if (mkdir(dirName, 0755) == -1){
        switch (errno) {
            case EEXIST:
                write(STDOUT_FILENO, "Directory already exists\n", strlen("Directory already exists\n"));
                break;
            default:
                write(STDOUT_FILENO, "An error occurred\n", 18);
            }
        }
    }

// cd
void changeDir(char *dirName){
    if(chdir(dirName) == -1){
        char* message = "Error Directory not found \n";
        write(STDOUT_FILENO, message, strlen(message));
    }
}

// cp
void copyFile(char *sourcePath, char *destinationPath){
    int src_fd, dst_fd;
    ssize_t num_read;
    char buffer[1024];
    struct stat statbuf;
    char* Dest;

    src_fd = open(sourcePath, O_RDONLY);
    if (src_fd == -1) {
        char* message = "Error opening source file \n";
        write(STDOUT_FILENO, message, strlen(message));
        return;
    }
    if (stat(destinationPath, &statbuf) == 0 && S_ISDIR(statbuf.st_mode)) {
        Dest = (char*)malloc(sizeof(char)*(strlen(sourcePath) + strlen(destinationPath) + 2));
        strcpy(Dest, destinationPath);
        strcat(Dest, "/");
        strcat(Dest, basename(sourcePath));
        Dest[strlen(sourcePath) + strlen(destinationPath)+1] = '\0';
    }else{
        Dest = (char*)malloc(sizeof(char)*(strlen(destinationPath))+1);
        strcpy(Dest, destinationPath);
        Dest[strlen(destinationPath)] = '\0';
    }
    
    dst_fd = open(Dest, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if (dst_fd == -1) {
            char* message = "Error opening destination file \n";
            write(STDOUT_FILENO, message, strlen(message));
            //close(src_fd);  // Close source file descriptor before returning
            return;
    }
    
    
    while((num_read = read(src_fd, buffer, sizeof(buffer))) > 0){
        if(write(dst_fd, buffer, num_read) != num_read){
            //"Error writing to destination file"
            break;
        }
    }
    close(src_fd);
    close(dst_fd);
    free(Dest);
} 

// mv
void moveFile(char *sourcePath, char *destinationPath){
    copyFile(sourcePath, destinationPath);
    remove(sourcePath);
}

// rm
void deleteFile(char *filename){
    if(remove(filename) == -1){
        char* message = "File not found \n";
        write(STDOUT_FILENO, message, strlen(message));
    }
}

// cat
void displayFile(char *filename){
    int src_fd;
    ssize_t num_read;

    char buffer[1024];
    src_fd = open(filename, O_RDONLY);
    if (src_fd == -1) {
        char* message = "File not found \n";
        write(STDOUT_FILENO, message, strlen(message));
        return;
    }
    while((num_read = read(src_fd, buffer, sizeof(buffer)))  > 0){
        if(write(STDOUT_FILENO, buffer, num_read) != num_read){
            //perror("Error writing to destination file");
            close(src_fd);
        
            return;
        }
    } 
}