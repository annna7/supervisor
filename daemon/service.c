#include <time.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <errno.h>
#include "service.h"
#include "existing_process_handler.h"
#include "utils.h"
#include "constants.h"
#include "global_state.h"

service_t service_open(pid_t pid) {
    int *argc = malloc(sizeof(int));
    char **argv = get_process_arguments(pid, argc);
    if (argv == NULL) {
        syslog(LOG_ERR, "Failed to get process arguments");
        return get_empty_service();
    }
    service_t service;
    service.pid = pid;
    strcpy(service.service_name, get_process_executable_name(pid));
    service.start_time = get_process_start_time(pid);
    strcpy(service.formatted_service_name, strdup(format_service_name(service.service_name, pid, service.start_time)));
    strcpy(service.program_path, get_process_path(pid));
    for (int i = 0; i < *argc; i++) {
        service.argv[i] = malloc(MAX_ARG_LENGTH);
        strncpy(service.argv[i], argv[i], MAX_ARG_LENGTH);
        service.argv[i][MAX_ARG_LENGTH - 1] = '\0';
    }
    service.argc = *argc;
    service.restart_times_left = 0;
    pthread_mutex_lock(&status_mutex);
    service.status = SUPERVISOR_STATUS_RUNNING;
    pthread_mutex_unlock(&status_mutex);
    service.is_opened = true;
    syslog(LOG_INFO, "Service %s opened", service.formatted_service_name);
    return service;
}

int service_kill(service_t *service) {
    if (!service->pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }

    // assert that service is not pending
    pthread_mutex_lock(&status_mutex);
    if (service->status == SUPERVISOR_STATUS_PENDING) {
        syslog(LOG_ERR, "Service is pending and cannot be killed. Use service-cancel instead!");
        pthread_mutex_unlock(&status_mutex);
        return -1;
    }
    pthread_mutex_unlock(&status_mutex);

    syslog(LOG_INFO, "Killing service %s", service->formatted_service_name);

    kill(service->pid, SIGKILL);

    int status;
    // deal with zombie processes and deallocate resources
    waitpid(service->pid, &status, 0);
    syslog(LOG_INFO, "Service %s killing", service->formatted_service_name);
    pthread_mutex_lock(&status_mutex);
    service->status = SUPERVISOR_STATUS_TERMINATED;
    pthread_mutex_unlock(&status_mutex);

    return 0;
}

int service_status(service_t* service) {
    if (!service->pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }
    pthread_mutex_lock(&status_mutex);
    int status = service->status;
    pthread_mutex_unlock(&status_mutex);
    return status;
}

// TODO: handle scheduling?
int service_cancel(service_t *service) {
    if (!service->pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }

    pthread_mutex_lock(&status_mutex);
    if (service->status == SUPERVISOR_STATUS_PENDING) {
        // service->status = SUPERVISOR_STATUS_TERMINATED;
        syslog(LOG_INFO, "Service %d was successfully cancelled!", service->pid);
        pthread_mutex_unlock(&status_mutex);
        return 0;
    } else {
        syslog(LOG_INFO, "Service %d isn't pending and can't be cancelled, use service-kill instead!", service->pid);
        pthread_mutex_unlock(&status_mutex);
        return -1;
    }
}

int service_resume(service_t *service) {
    if (!service->pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }

    pthread_mutex_lock(&status_mutex);
    if (service->status == SUPERVISOR_STATUS_PENDING) {
        // TODO: handle scheduling?
        syslog(LOG_ERR, "Don't know how to handle scheduling yet!");
        pthread_mutex_unlock(&status_mutex);
        return -1;
    } else if (service->status == SUPERVISOR_STATUS_STOPPED) {
        // conflict between SIGCHILD handler and manual resume
        // service->status = SUPERVISOR_STATUS_RUNNING;
        pthread_mutex_unlock(&status_mutex);
        kill(service->pid, SIGCONT);
        syslog(LOG_INFO, "Service %d was successfully resumed!", service->pid);
        return 0;
    } else {
        syslog(LOG_INFO, "Service %d isn't stopped and can't be resumed!", service->pid);
        pthread_mutex_unlock(&status_mutex);
        return -1;
    }
}

service_t service_create(const char * service_name, const char * program_path, const char ** argv, int argc, int flags, time_t start_time) {
    syslog(LOG_INFO, "Creating service %s", service_name);
    syslog(LOG_INFO, "Program path %s", program_path);
    service_t service;

    strncpy(service.service_name, service_name, MAX_SERVICE_NAME_LENGTH);
    service.service_name[MAX_SERVICE_NAME_LENGTH - 1] = '\0';

    strncpy(service.program_path, program_path, MAX_PROGRAM_PATH_LENGTH);
    service.program_path[MAX_PROGRAM_PATH_LENGTH - 1] = '\0';

    service.argc = argc < MAX_ARGS ? argc : MAX_ARGS;
    for (int i = 0; i < service.argc; i++) {
        service.argv[i] = malloc(MAX_ARG_LENGTH);
        strncpy(service.argv[i], argv[i], MAX_ARG_LENGTH);
        service.argv[i][MAX_ARG_LENGTH - 1] = '\0';
    }

    syslog(LOG_INFO, "In CREATE SERVICE, printing argv's");
    for (int i = 0; i < service.argc; i++) {
        syslog(LOG_INFO, "%s", service.argv[i]);
    }


    pthread_mutex_lock(&status_mutex);
    if ((flags & SUPERVISOR_FLAGS_CREATESTOPPED) == 1) {
        service.status = SUPERVISOR_STATUS_STOPPED;
        syslog(LOG_INFO, "Service %s will be created stopped", service_name);
    } else if ((flags & SUPERVISOR_FLAGS_CREATESTOPPED) > 1) {
        service.status = SUPERVISOR_STATUS_PENDING;
        syslog(LOG_INFO, "Service %s is pending for %d seconds", service_name, (flags & SUPERVISOR_FLAGS_CREATESTOPPED) - 1);
    }else {
        service.status = SUPERVISOR_STATUS_RUNNING;
        syslog(LOG_INFO, "%s %s %s", "Starting service", service_name, program_path);
    }
    pthread_mutex_unlock(&status_mutex);

    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork");
        return service;
    }
    if (pid == 0) {
        if (access(program_path, F_OK | X_OK) != 0) {
            syslog(LOG_ERR, "Failed to access program %s", program_path);
            syslog(LOG_ERR, "%d %s", errno, strerror(errno));
            return get_empty_service();
        }

        if ((flags & SUPERVISOR_FLAGS_CREATESTOPPED) == 1) {
            raise(SIGSTOP);
        }else if((flags & SUPERVISOR_FLAGS_CREATESTOPPED) > 0){
            sleep(flags & SUPERVISOR_FLAGS_CREATESTOPPED -1);
            service.status = SUPERVISOR_STATUS_RUNNING;
            syslog(LOG_INFO, "Service %s will start now", service_name);
        }

        execv(program_path, (char *const *) argv);
        syslog(LOG_ERR, "Failed to execv");
        syslog(LOG_ERR, "%d %s", errno, strerror(errno));
        return get_empty_service();
    }

    service.pid = pid;
    service.start_time = start_time;
    strcpy(service.formatted_service_name, strdup(format_service_name(service_name, pid, service.start_time)));
    syslog(LOG_INFO, "Service %s created with path %s", service.formatted_service_name, service.program_path);
    syslog(LOG_INFO, "Restart times: %d %d", flags, (flags & 16) & 0xF);
    service.restart_times_left = flags >> 16;
    service.is_opened = false;
    return service;
}

int service_restart(service_t *service) {
    syslog(LOG_INFO, "Just got in service_restart with pid %d", service->pid);
    pthread_mutex_lock(&status_mutex);
    if (service->status != SUPERVISOR_STATUS_CRASHED || service->restart_times_left == 0) {
        syslog(LOG_ERR, "INTERNAL ERR: Service %d isn't actually crashed or doesn't actually have any restart times left!", service->pid);
        pthread_mutex_unlock(&status_mutex);
        return -1;
    }
    pthread_mutex_unlock(&status_mutex);
    service->restart_times_left--;
    syslog(LOG_INFO, "Restarting service %s %s", service->formatted_service_name, service->program_path);
    service->pid = fork();
    if (service->pid < 0) {
        syslog(LOG_ERR, "Failed to fork");
        return -1;
    }
    if (service->pid == 0) {
        if (access(service->program_path, F_OK | X_OK) != 0) {
            syslog(LOG_ERR, "Failed to access program %s", service->program_path);
            syslog(LOG_ERR, "%d %s", errno, strerror(errno));
            return -1;
        }
        syslog(LOG_INFO, "RECREATE SERVICE PROGRAM PATH: %s", service->program_path);
        for (int i = 0; i < service->argc; i++) {
            syslog(LOG_INFO, "ARG %d: %s", i, service->argv[i]);
        }
        syslog(LOG_INFO, "ARGC: %d", service->argc);
        service->argv[service->argc] = NULL;

        execv(service->program_path, (char *const *) service->argv);
        syslog(LOG_ERR, "Failed to execv");
        syslog(LOG_ERR, "%d %s", errno, strerror(errno));
        return -1;
    }
    syslog(LOG_INFO, "Service %s restarted with pid %d", service->formatted_service_name, service->pid);
    pthread_mutex_lock(&status_mutex);
    service->status = SUPERVISOR_STATUS_RUNNING;
    pthread_mutex_unlock(&status_mutex);
    service->start_time = time(NULL);
    strcpy(service->formatted_service_name, strdup(format_service_name(service->service_name, service->pid, service->start_time)));
    return 0;
}

int service_suspend(service_t *service) {
    if (!service->pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }

    pthread_mutex_lock(&status_mutex);
    if (service->status == SUPERVISOR_STATUS_RUNNING) {
        service->status = SUPERVISOR_STATUS_STOPPED;
        pthread_mutex_unlock(&status_mutex);
        kill(service->pid, SIGSTOP);
        syslog(LOG_INFO, "Service %d was successfully suspended!", service->pid);
        return 0;
    } else {
        syslog(LOG_INFO, "Service %d isn't running and can't be suspended!", service->pid);
        pthread_mutex_unlock(&status_mutex);
        return -1;
    }
}

service_t get_empty_service() {
    service_t empty_service;
    empty_service.pid = 0;
    empty_service.start_time = 0;
    empty_service.service_name[0] = '\0';
    empty_service.program_path[0] = '\0';
    empty_service.formatted_service_name[0] = '\0';
    empty_service.argc = 0;
    empty_service.restart_times_left = 0;
    pthread_mutex_lock(&status_mutex);
    empty_service.status = 0;
    pthread_mutex_unlock(&status_mutex);
    return empty_service;
}