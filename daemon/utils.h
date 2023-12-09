#ifndef SUPERVISOR_UTILS_H
#define SUPERVISOR_UTILS_H

#include <fcntl.h>

#define MAX_SUPERVIORS 64
#define MAX_SERVICES_PER_INSTANCE 64

void parse_string(char *input_str, int* number_of_tokens, char **parsed_tokens);
void parse_formatted_service_name(const char* formatted_service_name, char *service_name, pid_t *pid, char *time_str);
char* format_service_name(const char *service_name, pid_t pid, time_t start_time);
#endif
