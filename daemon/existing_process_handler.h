#ifndef SUPERVISOR_EXISTING_PROCESS_HANDLER_H
#define SUPERVISOR_EXISTING_PROCESS_HANDLER_H

char **get_process_arguments(pid_t pid, int *argc);
char *get_process_path(pid_t pid);
char *get_process_executable_name(pid_t pid);
time_t get_process_start_time(pid_t pid);


#endif //SUPERVISOR_EXISTING_PROCESS_HANDLER_H
