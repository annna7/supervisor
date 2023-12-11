#include <time.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/wait.h>
#include <uv/errno.h>
#include "service.h"
#include "existing_process_handler.h"
#include "utils.h"

// TODO: add discriminator for open/create
// TODO: polling mechanism for open
service_t service_open(pid_t pid) {
    int *argc = malloc(sizeof(int));
    char **argv = get_process_arguments(pid, argc);
    if (argv == NULL) {
        syslog(LOG_ERR, "Failed to get process arguments");
        return get_empty_service();
    }
    service_t service = get_empty_service();
    service.pid = pid;
    service.service_name = get_process_executable_name(pid);
    service.start_time = get_process_start_time(pid);
    service.formatted_service_name = strdup(format_service_name(service.service_name, pid, service.start_time));
    service.program_path = get_process_path(pid);
    service.argv = (const char **) argv;
    service.argc = *argc;
    service.restart_times_left = 0;
    service.status = SUPERVISOR_STATUS_RUNNING;
    syslog(LOG_INFO, "Service %s opened", service.formatted_service_name);
    return service;
}

service_t service_create(const char * service_name, const char * program_path, const char ** argv, int argc, int flags, time_t start_time) {
    syslog(LOG_INFO, "Creating service %s", service_name);
    syslog(LOG_INFO, "Program path %s", program_path);
    service_t service = get_empty_service();
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

        service.status = SUPERVISOR_STATUS_PENDING;
        // TODO: scheduling mechanism for pending services?

        syslog(LOG_INFO, "%s %s %s", "Starting service", service_name, program_path);

        if (flags & SUPERVISOR_FLAGS_CREATESTOPPED) {
            syslog(LOG_INFO, "Service %s will be created stopped", service_name);
            service.status = SUPERVISOR_STATUS_STOPPED;
            raise(SIGSTOP);
        }

        service.status = SUPERVISOR_STATUS_RUNNING;
        execv(program_path, (char *const *) argv);
        syslog(LOG_ERR, "Failed to execv");
        syslog(LOG_ERR, "%d %s", errno, strerror(errno));
        return get_empty_service();
    }

    syslog(LOG_INFO, "Service %s started with pid %d", service_name, pid);
    service.pid = pid;
    service.service_name = service_name;
    service.start_time = start_time;
    service.formatted_service_name = strdup(format_service_name(service_name, pid, service.start_time));
    service.program_path = program_path;
    service.argv = argv;
    service.argc = argc;
    service.restart_times_left = (flags & SUPERVISOR_FLAGS_RESTARTTIMES(0xF)) >> 16;

    return service;
}

int service_kill(service_t service) {
    if (!service.pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }

    // assert that service is not pending
    if (service.status == SUPERVISOR_STATUS_PENDING) {
        syslog(LOG_ERR, "Service is pending and cannot be killed. Use service-cancel instead!");
        return -1;
    }

    syslog(LOG_INFO, "Killing service %s", service.formatted_service_name);

    kill(service.pid, SIGKILL);

    int status;
    // deal with zombie processes and deallocate resources
    waitpid(service.pid, &status, 0);
    syslog(LOG_INFO, "Service %s killing", service.formatted_service_name);
    service.status = SUPERVISOR_STATUS_KILLED;

    return 0;
}

int service_status(service_t service) {
    if (!service.pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }
    return service.status;
}

// TODO: handle scheduling?
int service_cancel(service_t service) {
    if (!service.pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }

    if (service.status == SUPERVISOR_STATUS_PENDING) {
        service.status = SUPERVISOR_STATUS_STOPPED;
        syslog(LOG_INFO, "Service %d was successfully cancelled!", service.pid);
        return 0;
    } else {
        syslog(LOG_INFO, "Service %d isn't pending and can't be cancelled, use service-kill instead!", service.pid);
        return -1;
    }
}

int service_resume(service_t service) {
    if (!service.pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }

    if (service.status == SUPERVISOR_STATUS_PENDING) {
        // TODO: handle scheduling?
        syslog(LOG_ERR, "Don't know how to handle scheduling yet!");
        return -1;
    } else if (service.status == SUPERVISOR_STATUS_STOPPED) {
        service.status = SUPERVISOR_STATUS_RUNNING;
        kill(service.pid, SIGCONT);
        syslog(LOG_INFO, "Service %d was successfully resumed!", service.pid);
        return 0;
    } else {
        syslog(LOG_INFO, "Service %d isn't stopped and can't be resumed!", service.pid);
        return -1;
    }
}

int service_suspend(service_t service) {
    if (!service.pid) {
        syslog(LOG_ERR, "No such service");
        return -1;
    }

    if (service.status == SUPERVISOR_STATUS_RUNNING) {
        service.status = SUPERVISOR_STATUS_STOPPED;
        kill(service.pid, SIGSTOP);
        // TODO: check if the service is actually stopped (proc/pid/status ?)
        syslog(LOG_INFO, "Service %d was successfully suspended!", service.pid);
        return 0;
    } else {
        syslog(LOG_INFO, "Service %d isn't running and can't be suspended!", service.pid);
        return -1;
    }
}

service_t get_empty_service() {
    service_t empty_service;
    empty_service.service_name = NULL;
    empty_service.program_path = NULL;
    empty_service.argv = NULL;
    empty_service.argc = 0;
    empty_service.pid = 0;
    empty_service.start_time = 0;
    empty_service.restart_times_left = 0;
    return empty_service;
}