#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "utils.h"

void parse_string(char *input_string, int* number_of_tokens, char **parsed_tokens) {
    char *token;
    *number_of_tokens = 0;

    token = strtok(input_string, " ");
    while (token != NULL) {
        parsed_tokens[*number_of_tokens] = malloc(strlen(token) + 1);
        if (parsed_tokens[*number_of_tokens]) {
            strcpy(parsed_tokens[*number_of_tokens], token);
        }
        token = strtok(NULL, " ");
        (*number_of_tokens)++;
    }
}

char* format_service_name(const char *service_name, pid_t pid, time_t start_time) {
    char *format_str = malloc(256 * sizeof(char));
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%c", localtime(&start_time));
    snprintf(format_str, 256, "%s_%d_%s", service_name, pid, time_str);
    return format_str;
}

pid_t extract_pid_from_formatted_service_name(const char* formatted_service_name) {
    char *service_name = malloc(strlen(formatted_service_name) + 1);
    pid_t pid;
    char *time_str = malloc(strlen(formatted_service_name) + 1);
    parse_formatted_service_name(formatted_service_name, service_name, &pid, time_str);
    free(service_name);
    free(time_str);
    return pid;
}

void parse_formatted_service_name(const char* formatted_service_name, char *service_name, pid_t *pid, char *time_str) {
    char *token;
    int number_of_tokens = 0;

    char *formatted_service_name_copy = malloc(strlen(formatted_service_name) + 1);
    strcpy(formatted_service_name_copy, formatted_service_name);  // Copy the content

    token = strtok(formatted_service_name_copy, "_");
    while (token != NULL) {
        switch (number_of_tokens) {
            case 0:
                strcpy(service_name, token);
                break;
            case 1:
                *pid = atoi(token);
                break;
            case 2:
                strcpy(time_str, token);
                break;
            default:
                break;
        }
        token = strtok(NULL, "_");
        number_of_tokens++;
    }

    free(formatted_service_name_copy);
}

void append_service_status_to_string(int service_status, char *response_str) {
    switch (service_status) {
        case SUPERVISOR_STATUS_RUNNING: {
            strcat(response_str, "Running");
            break;
        }
        case SUPERVISOR_STATUS_PENDING: {
            strcat(response_str, "Pending");
            break;
        }
        case SUPERVISOR_STATUS_STOPPED: {
            strcat(response_str, "Stopped");
            break;
        }
        case SUPERVISOR_STATUS_TERMINATED: {
            strcat(response_str, "Killed");
            break;
        }
        case SUPERVISOR_STATUS_CRASHED: {
            strcat(response_str, "Crashed");
            break;
        }
        case SUPERVISOR_STATUS_CANCELED: {
            strcat(response_str, "Canceled");
            break;
        }
        default:
            strcat(response_str, "Unknown status");
    }
}