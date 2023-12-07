#ifndef SUPERVISOR_SUPERVISOR_H
#define SUPERVISOR_SUPERVISOR_H

#include "service.h"

#define MAX_SUPERVIORS 100
#define MAX_SERVICES_PER_INSTANCE 100

typedef struct {
    int instance;
    service_t services[MAX_SERVICES_PER_INSTANCE];
} supervisor_t;

supervisor_t* supervisor_init(int instance);
supervisor_t* supervisor_get(int instance);
int supervisor_close(supervisor_t*);
int supervisor_list(supervisor_t* supervisor, const char *** service_names, unsigned int * count);
int supervisor_freelist(supervisor_t* supervisor, const char ** service_names, int count);

#endif
