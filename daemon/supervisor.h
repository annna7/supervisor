#ifndef SUPERVISOR_SUPERVISOR_H
#define SUPERVISOR_SUPERVISOR_H

#include "service.h"
#include "utils.h"

typedef struct {
    int instance;
    service_t services[MAX_SERVICES_PER_INSTANCE];
} supervisor_t;

supervisor_t* supervisor_init(int instance);
supervisor_t* supervisor_get(int instance);
void list_supervisors();
int get_free_service_index(supervisor_t *supervisor);
int supervisor_close(supervisor_t*);
int supervisor_list(supervisor_t* supervisor, const char *** service_names, unsigned int * count);
int supervisor_freelist(supervisor_t* supervisor, const char ** service_names, int count);
int supervisor_create_service_wrapper(supervisor_t* supervisor, const char * service_name, const char * program_path, const char ** argv, int argc, int flags);
int supervisor_close_service_wrapper(supervisor_t* supervisor, pid_t pid);

#endif
