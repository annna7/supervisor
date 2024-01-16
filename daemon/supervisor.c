#include <stdlib.h>
#include <syslog.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include "supervisor.h"
#include "global_state.h"

supervisor_t* supervisors[MAX_SUPERVISORS] = {NULL};
typedef struct {
    supervisor_t * supervisor;
    int service_index;
    int* scheduling_pipe_fd;
} schedulingThreadArgs;


void list_supervisors() {
    bool are_supervisors = false;
    for (int i = 0; i < MAX_SUPERVISORS; i++) {
        if (supervisors[i]) {
//            char *response = malloc(sizeof(char) * RESPONSE_STR_SIZE);
//            char  response[RESPONSE_STR_SIZE] ;
//            sprintf(response, "Supervisor %d\n", i);
//            sprintf(global_response_str + strlen(global_response_str), "Supervisor %d\n", i);
//            strcat(global_response_str, response);
//            free(response);
            append_to_global_response_str("Supervisor %d\n", i);
            are_supervisors = true;
        }
    }
    if (!are_supervisors) {
        strcpy(global_response_str, "No supervisors at the moment!\n");
    }
}

supervisor_t* supervisor_init(int instance) {
    if (instance < 0 || instance >= MAX_SUPERVISORS) {
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
    if (instance < 0 || instance >= MAX_SUPERVISORS) {
        return NULL;
    }
    if(supervisors[instance] == NULL) {
//        char  response[RESPONSE_STR_SIZE] ;
//        sprintf(response, "Supervisor %d does not exist\n", instance);
//        strcpy(global_response_str, response);
        append_to_global_response_str("Supervisor %d does not exist\n", instance);
//        char *response = malloc(sizeof(char) * RESPONSE_STR_SIZE);
//        sprintf(response, "Supervisor %d does not exist\n", instance);
//        strcat(global_response_str, response);
//        free(response);
//        sprintf(global_response_str + strlen(global_response_str), "Supervisor %d does not exist\n", instance);
    }
    return supervisors[instance];
}

int supervisor_close(supervisor_t* supervisor) {
    if (!supervisor) {
        return -1;
    }

    append_to_global_response_str("Supervisor %d has been closed\n", supervisor->instance);
//    char response[RESPONSE_STR_SIZE] ;
//    sprintf(response, "Supervisor %d has been closed\n", supervisor->instance);
//    strcpy(global_response_str, response);
//    char *response = malloc(sizeof(char) * RESPONSE_STR_SIZE);
//    sprintf(response, "Supervisor %d has been closed\n", supervisor->instance);
//    strcat(global_response_str, response);
//    free(response);
//    sprintf(global_response_str + strlen(global_response_str), "Supervisor %d has been closed\n", supervisor->instance);
//    global_response_str[strlen(global_response_str)] = '\0';
    const char *** service_names = malloc(sizeof(char**));
    unsigned int * count = malloc(sizeof(unsigned int));

    supervisor_list(supervisor, service_names, count);

    // convert service_names to pid_array
    pid_t * pid_array = malloc(sizeof(pid_t) * *count);
    for (int i = 0; i < *count; i++) {
        pid_array[i] = extract_pid_from_formatted_service_name((*service_names)[i]);
    }

    supervisor_freelist(supervisor, pid_array, (int) *count);
    supervisors[supervisor->instance] = NULL;
    free(supervisor);
    return 0;
}
void* scheduling_thread_function(void *args){
    syslog(LOG_INFO, "Starting scheduling_thread_function");
    schedulingThreadArgs *threadArgs = (schedulingThreadArgs *) args;
    supervisor_t *supervisor = threadArgs->supervisor;
    int index = threadArgs->service_index;
    int* scheduling_pipe_fd = threadArgs->scheduling_pipe_fd;
    char buf[128];

    while (1){
        ssize_t nbytes = read(scheduling_pipe_fd[0], buf, sizeof(buf));
        if (nbytes > 0) {
            syslog(LOG_INFO, "Received scheduling_thread buffer size: %s", buf);
            char *status = strtok(buf, " ");
            syslog(LOG_INFO, "Received scheduling_thread status: %s", status);

            if (status != NULL) {
                syslog(LOG_INFO, "Inainte de mutex thread");
                pthread_mutex_lock(&status_mutex);
                supervisor->services[index].status = SUPERVISOR_STATUS_RUNNING;
                pthread_mutex_unlock(&status_mutex);
                syslog(LOG_INFO, "Am schimbat status");
                break;
            }

            buf[nbytes] = '\0';
        } else if (nbytes < 0) {
            if (errno != EAGAIN && errno != 9) {
                if (errno == 9) {
                    syslog(LOG_ERR, "iar eu");
                } else {
                    syslog(LOG_ERR, "Failed to read from pipe");
                    syslog(LOG_ERR, "Error: %s %d", strerror(errno), errno);
                }
            }
            sleep(1);
        } else if (nbytes == 0) {
            syslog(LOG_ERR, "Pipe closed");
            break;
        }
    }
    free(scheduling_pipe_fd);
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

    int *scheduling_pipe_fd = (int *)malloc(2 * sizeof(int));
    if(scheduling_pipe_fd == NULL){
        perror("malloc_scheduling_pipe_fd");
        exit(EXIT_FAILURE);
    }
    // setting up the pipe for communication between scheduled thread and daemon
    if (pipe(scheduling_pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    if (fcntl(scheduling_pipe_fd[1], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    if (fcntl(scheduling_pipe_fd[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    service_t new_service = service_create(service_name, program_path, argv, argc, flags, time(NULL), scheduling_pipe_fd);
    if (!new_service.pid) {
        syslog(LOG_ERR, "Failed to create service");
        return -1;
    }
    syslog(LOG_INFO, "%s\n", new_service.formatted_service_name);
    supervisor->services[i] = new_service;
    syslog(LOG_INFO, "Just created service with pid %d and status %d", supervisor->services[i].pid, supervisor->services[i].status);

    if(new_service.status == SUPERVISOR_STATUS_PENDING) {

        //Creating thread that waits for the status to change
        pthread_t scheduling_thread;
        schedulingThreadArgs args;
        args.supervisor = supervisor;
        args.service_index = i;
        args.scheduling_pipe_fd = scheduling_pipe_fd;
        if(pthread_mutex_trylock(&status_mutex) == 0){
            pthread_mutex_unlock(&status_mutex);
            syslog(LOG_INFO, "Mutex NU E blocat inainte de thread");
        } else{
            syslog(LOG_INFO, "Mutex blocat inainte de thread");
        }
        if (pthread_create(&scheduling_thread, NULL, scheduling_thread_function, (void *) &args) != 0) {
            perror("pthread_create");
            exit(EXIT_FAILURE);
        }
    }
    else{
        syslog(LOG_INFO, "Nu Facem threadul");
    }

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
    if (!new_service.pid) {
        syslog(LOG_ERR, "Failed to open service");
        return get_empty_service();
    }
    supervisor->services[i] = new_service;
    return new_service;
}

int supervisor_remove_service_wrapper(supervisor_t* supervisor, pid_t pid) {
    if (!supervisor) {
        return -1;
    }
    int i = get_service_index_from_pid(supervisor, pid);
    if (i == -1) {
        syslog(LOG_ERR, "Service %d not found", pid);
        return -1;
    }
    for (int j = 0; j < supervisor->services[i].argc; j++) {
        free((char*) supervisor->services[i].argv[j]);
    }
    supervisor->services[i] = get_empty_service();
    return 0;
}

int supervisor_send_command_to_existing_service_wrapper(supervisor_t* supervisor, pid_t pid, int command) {
    if (!supervisor) {
        return -1;
    }
    syslog(LOG_INFO, "IN supervisor_send_command_to_existing_service_wrapper");
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
    for (int i = 0; i < MAX_SUPERVISORS; i++) {
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
        if (supervisor->services[i].pid == 0) {
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
    syslog(LOG_INFO, "In supervospr_list");
    *service_names = malloc(sizeof(char*) * MAX_SERVICES_PER_INSTANCE);
    if (!*service_names) {
        syslog(LOG_ERR, "malloc failed");
        return -1;
    }
    *count = 0;
    for (int i = 0; i < MAX_SERVICES_PER_INSTANCE; i++) {
        if (supervisor->services[i].pid) {
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