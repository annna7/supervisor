#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include "supervisor.h"

supervisor_t* supervisors[MAX_SUPERVIORS] = {NULL};

void list_supervisors() {
    syslog(LOG_INFO, "list_supervisors");
    printf("list_supervisors\n");
    for (int i = 0; i < MAX_SUPERVIORS; i++) {
        if (supervisors[i]) {
            printf("%d\n", i);
            syslog(LOG_INFO, "supervisor %d", i);
        }
    }
}

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
    for (int i = 0; i < MAX_SERVICES_PER_INSTANCE; i++) {
        supervisor->services[i] = get_empty_service();
    }
    syslog(LOG_INFO, "%s %d", "supervisor_init", instance);
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
    supervisors[supervisor->instance] = NULL;
    free(supervisor);
    return 0;
}

int supervisor_create_service_wrapper(supervisor_t* supervisor, const char * service_name, const char * program_path, const char ** argv, int argc, int flags, pid_t *new_pid) {
    if (!supervisor) {
        return -1;
    }
    int i = get_free_service_index(supervisor);
    if (i == -1) {
        syslog(LOG_ERR, "No space for new service");
        return -1;
    }

    syslog(LOG_INFO, "service_create called with service_name: %s, program_path: %s, argc: %d, flags: %d", service_name, program_path, argc, flags);
    for(int i = 0; i < argc; i++) {
        syslog(LOG_INFO, "argv[%d]: %s", i, argv[i]);
    }

    service_t new_service = service_create(service_name, program_path, argv, argc, flags);
    if (new_service.service_name == NULL) {
        syslog(LOG_ERR, "Failed to create service");
        return -1;
    }
    syslog(LOG_INFO, "%s\n", new_service.service_name);
    supervisor->services[i] = new_service;
    *new_pid = new_service.pid;
    return 0;
}

int supervisor_close_service_wrapper(supervisor_t* supervisor, pid_t pid) {
    if (!supervisor) {
        return -1;
    }
    syslog(LOG_INFO, "supervisor_close_service_wrapper called with pid: %d", pid);
    syslog(LOG_INFO, "supervisor->instance: %d", supervisor->instance);
    for (int i = 0; i < MAX_SERVICES_PER_INSTANCE; i++) {
        if (supervisor->services[i].pid == pid) {
            service_close(supervisor->services[i]);
            free((char*) supervisor->services[i].formatted_service_name);
            supervisor->services[i] = get_empty_service();
            return 0;
        }
    }
    return 0;
}

int get_free_service_index(supervisor_t *supervisor) {
    for (int i = 0; i < MAX_SERVICES_PER_INSTANCE; i++) {
        if (supervisor->services[i].service_name == NULL) {
            return i;
        }
    }
    return -1;
}

int supervisor_list(supervisor_t* supervisor, const char *** service_names, unsigned int * count) {
    if (!supervisor) {
        return -1;
    }
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
