#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <linux/limits.h>
#include <fcntl.h>
#include "existing_process_handler.h"
#include "constants.h"

#include <time.h>
#include <syslog.h>


long get_ticks_per_second() {
    return sysconf(_SC_CLK_TCK);
}

time_t get_system_up_time(){
    char path[40], buffer[1024];
    sprintf(path, "/proc/uptime");
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        perror("Error reading file");
        fclose(file);
        return -1;
    }

    char *token;
    const char *delimiter = " ";
    time_t up_time = 0;

    token = strtok(buffer, delimiter);
    up_time = strtoll(token, NULL, 10);
    fclose(file);

    return up_time;

}

// TODO: why is this returning the wrong date?
time_t get_process_start_time(pid_t pid) {

    time_t system_up_time = get_system_up_time();
    time_t current_time;
    time(&current_time);


    char path[40], buffer[1024];
    sprintf(path, "/proc/%d/stat", pid);
    FILE *file = fopen(path, "r");
    if (!file) {
        perror("Error opening file");
        return -1;
    }

    if (fgets(buffer, sizeof(buffer), file) == NULL) {
        perror("Error reading file");
        fclose(file);
        return -1;
    }

    // start time is the 22nd field in /proc/[pid]/stat
    char *token;
    const char *delimiter = " ";
    int count = 0;
    time_t start_time = 0;

    token = strtok(buffer, delimiter);
    while (token != NULL) {
        if (++count == 22) {
            start_time = strtoll(token, NULL, 10) / get_ticks_per_second();
            break;
        }
        token = strtok(NULL, delimiter);
    }

    fclose(file);
    start_time = current_time - (system_up_time - start_time);
    return start_time;
}

char *get_process_path(pid_t pid) {
    char exe_path[40], *buffer;
    ssize_t len;
    sprintf(exe_path, "/proc/%d/exe", pid);

    buffer = malloc(PATH_MAX);
    if (!buffer) {
        perror("Malloc failed");
        return NULL;
    }

    len = readlink(exe_path, buffer, PATH_MAX - 1);
    if (len == -1) {
        perror("Error reading link");
        free(buffer);
        return NULL;
    }

    buffer[len] = '\0';
    return buffer;
}

char *get_process_executable_name(pid_t pid) {
    char *path = get_process_path(pid);
    if (!path) {
        return NULL;
    }

    char *name = strrchr(path, '/');
    if (!name) {
        return NULL;
    }

    name++;
    return name;
}

char **get_process_arguments(pid_t pid, int *argc) {
    char path[40], *arg_data;
    sprintf(path, "/proc/%d/cmdline", pid);

    int fd = open(path, O_RDONLY);
    if (fd == -1) {
        perror("Error opening cmdline");
        return NULL;
    }

    arg_data = malloc(MAX_ARG_LEN);
    if (!arg_data) {
        perror("Malloc failed");
        close(fd);
        return NULL;
    }

    ssize_t bytes_read = read(fd, arg_data, MAX_ARG_LEN);
    if (bytes_read == -1) {
        perror("Error reading cmdline");
        close(fd);
        free(arg_data);
        return NULL;
    }

    close(fd);

    char **argv = malloc(MAX_ARGS * sizeof(char *));
    if (!argv) {
        perror("Malloc failed");
        free(arg_data);
        return NULL;
    }

    int arg_count = 0;
    char *token = strtok(arg_data, "\0");
    while (token != NULL && arg_count < MAX_ARGS) {
        argv[arg_count++] = strdup(token);
        token = strtok(NULL, "\0");
    }

    *argc = arg_count;
    return argv;
}

