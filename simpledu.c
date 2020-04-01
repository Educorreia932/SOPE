#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include <sys/types.h> 
#include <sys/wait.h> 
#include <unistd.h> 
#include <errno.h> 
#include <stdbool.h>
#include <limits.h>

#include "flags.h"

#define READ 0
#define WRITE 1

int main(int argc, char* argv[], char* envp[]) {

    DIR *dirp;
    struct dirent *direntp;
    struct stat stat_buf;
    char *str;
    char name[300]; 
    flags* c = flags_constructor();

    if (argc > 9) {
        perror("Usage: simpledu -l [path] [-a] [-b] [-B size] [-L] [-S] [--max-depth=N]");
        exit(1);
    }

    parse_flags(argc, argv, c);
    //print_flags(c);

    if ((dirp = opendir(c->path)) == NULL) {
        perror(c->path);
        exit(2);
    } 
    
    while ((direntp = readdir(dirp)) != NULL) {
        int fd[2];

        // Format path for each directory/file
        sprintf(name, "%s/%s", c->path, direntp->d_name);

        if (lstat(name, &stat_buf) == -1) {
            perror("lstat ERROR");
            exit(3);
        }

        int size;
        double multiplier = stat_buf.st_blocks != 0? 512.0 / c->size : 0;

        if (c->bytes) 
            size = stat_buf.st_size;

        else
            size = stat_buf.st_blocks != 0? stat_buf.st_blocks * multiplier : multiplier;           

        if (S_ISLNK(stat_buf.st_mode))
            break;

        // File
        if (S_ISREG(stat_buf.st_mode) && c->max_depth > 0 && c->all) {
            char str[200];

            sprintf(str, "%-7u %s\n", size, name);

            write(STDOUT_FILENO, str, strlen(str));

            close(fd[READ]);
            write(fd[WRITE], &size, sizeof(int*));
            close(fd[WRITE]);
        }
        
        // Directory
        else if (S_ISDIR(stat_buf.st_mode)) {
            pid_t pid;
            int status;

            if (name[strlen(name) - 1] == '.') // Fix this to allow for any folder ended in .
                continue;

            else
                pid = fork();

            // Parent
            if (pid > 0) {
                int* file_size;

                wait(&status);

                close(fd[READ]);
                write(fd[WRITE], file_size, sizeof(int*));
                close(fd[WRITE]);

                if (c->max_depth > 0)
                    printf("%-7u %s\n", size, name);
            }

            // Child
            else if (pid == 0) {
                char max_depth[50];
                sprintf(max_depth, "--max-depth=%u", c->max_depth - 1);

                char all[3] = "-a";

                if (!c->all)
                    all[0] = '\0'; // Empty the string, set it to ""

                char* argv_[5] = {"simpledu", name, max_depth, all, NULL};

                if (execve("simpledu", argv_, envp) == -1)
                    printf("Error in exec %s\n", name);
            }

            // Error
            else {
                perror("Error in fork\n");

                exit(3);
            }
        }
    }

    closedir(dirp); 

    exit(0);
}