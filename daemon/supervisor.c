#include "supervisor.h"
#include <stdlib.h>

supervisor_t* supervisors[MAX_SUPERVIORS];

supervisor_t* supervisor_init(int instance) {
    if (instance < 0 || instance >= MAX_SUPERVIORS) {
        return NULL;
    }
    if (supervisors[instance]) {
        return NULL;
    }
    supervisor_t* supervisor = malloc(sizeof(supervisor_t));
    if (!supervisor) {
        return NULL;
    }
    supervisor->instance = instance;
    supervisors[instance] = supervisor;
    return supervisor;
}

supervisor_t* supervisor_get(int instance) {
    if (instance < 0 || instance >= MAX_SUPERVIORS) {
        return NULL;
    }
    return supervisors[instance];
}

int supervisor_close(supervisor_t* supervisor) {
    if (!supervisor) {
        return -1;
    }
    if (supervisors[supervisor->instance] != supervisor) {
        return -1;
    }
    free(supervisor);
    supervisors[supervisor->instance] = NULL;
    return 0;
}

int supervisor_list(supervisor_t* supervisor, const char *** service_names, unsigned int * count) {
    if (!supervisor) {
        return -1;
    }
    // TODO: This should be done from the CLI
//    if (supervisors[supervisor->instance] != supervisor) {
//        return -1;
//    }
    *service_names = malloc(sizeof(char*) * MAX_SERVICES_PER_INSTANCE);
    if (!*service_names) {
        return -1;
    }
    *count = 0;
    for (int i = 0; i < MAX_SERVICES_PER_INSTANCE; i++) {
        if (supervisor->services[i].service_name) {
            (*service_names)[*count] = supervisor->services[i].service_name;
            (*count)++;
        }
    }
    return 0;
}

int supervisor_freelist(supervisor_t* supervisor, const char ** service_names, int count) {
    if (!supervisor) {
        return -1;
    }
    // TODO: This should be done from the CLI
//    if (supervisors[supervisor->instance] != supervisor) {
//        return -1;
//    }
    for (int i = 0; i < count; i++) {
        // TODO: check if service_names[i] is in supervisor->services
        // TODO: close service
        // TODO: remove service from supervisor->services
        // TODO: free service_names[i]
    }
    return 0;
}
