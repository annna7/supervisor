#include "utils.h"

#include <string.h>
#include <stdlib.h>

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