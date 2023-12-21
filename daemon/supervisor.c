#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <stdio.h>
#include <time.h>
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
    const char *** service_names = malloc(sizeof(char**));
    unsigned int * count = malloc(sizeof(unsigned int));

    supervisor_list(supervisor, service_names, count);

    // convert service_names to pid_array
    pid_t * pid_array = malloc(sizeof(pid_t) * *count);
    for (int i = 0; i < *count; i++) {
        pid_array[i] = extract_pid_from_formatted_service_name((*service_names)[i]);
    }

    // TODO: close all services first (freelist)
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

    service_t new_service = service_create(service_name, program_path, argv, argc, flags, time(NULL));
    if (new_service.service_name == NULL) {
        syslog(LOG_ERR, "Failed to create service");
        return -1;
    }
    syslog(LOG_INFO, "%s\n", new_service.formatted_service_name);
    supervisor->services[i] = new_service;
    syslog(LOG_INFO, "Just created service with pid %d and status %d", supervisor->services[i].pid, supervisor->services[i].status);
    *new_pid = new_service.pid;
    return 0;
}

service_t supervisor_open_service_wrapper(supervisor_t* supervisor, pid_t pid) {
    if (!supervisor) {
        return get_empty_service();
    }
    int i = get_free_service_index(supervisor);
    if (i == -1) {
        syslog(LOG_ERR, "No space for new service");
        return get_empty_service();
    }
    service_t new_service = service_open(pid);
    if (new_service.service_name == NULL) {
        syslog(LOG_ERR, "Failed to open service");
        return get_empty_service();
    }
    supervisor->services[i] = new_service;
    return new_service;
}

int supervisor_remove_service_wrapper(supervisor_t* supervisor, pid_t pid) {
//    syslog(LOG_INFO, "Removing service %d", pid);
    if (!supervisor) {
        return -1;
    }
    int i = get_service_index_from_pid(supervisor, pid);
    if (i == -1) {
        syslog(LOG_ERR, "Service %d not found", pid);
        return -1;
    }
    free((char*) supervisor->services[i].formatted_service_name);
    supervisor->services[i] = get_empty_service();
    return 0;
}

int supervisor_send_command_to_existing_service_wrapper(supervisor_t* supervisor, pid_t pid, int command) {
    if (!supervisor) {
        return -1;
    }
    int i = get_service_index_from_pid(supervisor, pid);
    if (i == -1) {
        syslog(LOG_ERR, "Service %d not found", pid);
        return -1;
    }
    switch (command) {
        case KILL_SERVICE: {
            int error = service_kill(&supervisor->services[i]);
            if (!error) {
                return error;
            }
            return supervisor_remove_service_wrapper(supervisor, pid);
        }
        case STATUS_SERVICE:
            return service_status(&supervisor->services[i]);
        case SUSPEND_SERVICE: {
            int res = service_suspend(&supervisor->services[i]);
            syslog(LOG_INFO, "Service %d suspended with %d", pid, res);
            return res;
        }
        case RESUME_SERVICE:
            return service_resume(&supervisor->services[i]);
        case CANCEL_SERVICE:
            // TODO: pending
            return service_cancel(&supervisor->services[i]);
        default:
            syslog(LOG_ERR, "Invalid command %d", command);
            return -1;
    }
}

int get_service_index_from_service_name(supervisor_t* supervisor, const char *formatted_service_name) {
    for (int i = 0; i < MAX_SERVICES_PER_INSTANCE; ++i) {
        if (strcmp(supervisor->services[i].formatted_service_name, formatted_service_name) == 0) {
            return i;
        }
    }
    return -1;
}

int get_supervisor_instance_from_service_pid(pid_t pid) {
    for (int i = 0; i < MAX_SUPERVIORS; i++) {
        if (supervisors[i]) {
            for (int j = 0; j < MAX_SERVICES_PER_INSTANCE; j++) {
                if (supervisors[i]->services[j].pid == pid) {
                    return i;
                }
            }
        }
    }
    return -1;
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
        syslog(LOG_ERR, "supervisor_list: supervisor is NULL");
        return -1;
    }
    *service_names = malloc(sizeof(char*) * MAX_SERVICES_PER_INSTANCE);
    if (!*service_names) {
        syslog(LOG_ERR, "malloc failed");
        return -1;
    }
    *count = 0;
    for (int i = 0; i < MAX_SERVICES_PER_INSTANCE; i++) {
        if (supervisor->services[i].service_name) {
            syslog(LOG_INFO, "supervisor_list: %s %d\n", supervisor->services[i].formatted_service_name, supervisor->services[i].pid);
            (*service_names)[*count] = malloc(sizeof(char) * (strlen(supervisor->services[i].formatted_service_name) + 1)); // +1 for the null terminator
            strcpy((*service_names)[*count], supervisor->services[i].formatted_service_name);
            (*count)++;
        }
    }
    return 0;
}

int supervisor_freelist(supervisor_t* supervisor, pid_t* pid_array, int count) {
    if (!supervisor) {
        return -1;
    }

    for (int i = 0; i < count; i++) {
        int index = get_service_index_from_pid(supervisor, pid_array[i]);
        if (index == -1) {
            syslog(LOG_ERR, "supervisor_freelist: service %d not found", pid_array[i]);
            return -1;
        }
        supervisor_remove_service_wrapper(supervisor, pid_array[i]);
    }

    free(pid_array);
    return 0;
}

int get_service_index_from_pid(supervisor_t* supervisor, pid_t pid) {
    for (int i = 0; i < MAX_SERVICES_PER_INSTANCE; i++) {
        if (supervisor->services[i].pid == pid) {
            return i;
        }
    }
    return -1;
}