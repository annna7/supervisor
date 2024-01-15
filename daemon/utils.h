#ifndef SUPERVISOR_UTILS_H
#define SUPERVISOR_UTILS_H

#include <fcntl.h>
#include "constants.h"

void parse_string(char *input_str, int* number_of_tokens, char **parsed_tokens);
pid_t extract_pid_from_formatted_service_name(const char* formatted_service_name);
void parse_formatted_service_name(const char* formatted_service_name, char *service_name, pid_t *pid, char *time_str);
char* format_service_name(const char *service_name, pid_t pid, time_t start_time);
void append_service_status_to_string(int service_status, char *response_str);
#endif
