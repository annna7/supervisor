#ifndef SUPERVISOR_SUPERVISOR_H
#define SUPERVISOR_SUPERVISOR_H

#include "service.h"
#include "constants.h"
#include "utils.h"

typedef struct {
    int instance;
    service_t services[MAX_SERVICES_PER_INSTANCE];
} supervisor_t;

extern supervisor_t* supervisors[];
supervisor_t* supervisor_init(int instance);
supervisor_t* supervisor_get(int instance);
service_t supervisor_open_service_wrapper(supervisor_t* supervisor, pid_t pid);
bool list_supervisors();
int get_supervisor_instance_from_service_pid(pid_t pid);
int get_service_index_from_pid(supervisor_t* supervisor, pid_t pid);
int get_service_index_from_service_name(supervisor_t* supervisor, const char *formatted_service_name);
int get_free_service_index(supervisor_t *supervisor);
int supervisor_close(supervisor_t*);
int supervisor_list(supervisor_t* supervisor, const char *** service_names, unsigned int * count);
int supervisor_freelist(supervisor_t* supervisor, pid_t* pid_array, int count);
int supervisor_create_service_wrapper(supervisor_t* supervisor, const char * service_name, const char * program_path, const char ** argv, int argc, int flags, pid_t *new_pid);
int supervisor_send_command_to_existing_service_wrapper(supervisor_t* supervisor, pid_t pid, int command);
int supervisor_remove_service_wrapper(supervisor_t* supervisor, pid_t pid);
#endif
