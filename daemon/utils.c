#include "utils.h"

#include <string.h>
#include <stdlib.h>
#include <time.h>
#include <stdio.h>


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

char* format_service_name(const char *service_name, pid_t pid) {
    char *format_str = malloc(256 * sizeof(char));
    char time_str[64];
    time_t now = time(NULL);
    strftime(time_str, sizeof(time_str), "%c", localtime(&now));
    snprintf(format_str, sizeof(format_str), "%s_%d_%s", service_name, pid, time_str);
    return format_str;
}


void parse_formatted_service_name(const char* formatted_service_name, char *service_name, pid_t *pid, char *time_str) {
    char *token;
    int number_of_tokens = 0;

    char *formatted_service_name_copy = malloc(strlen(formatted_service_name) + 1);

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