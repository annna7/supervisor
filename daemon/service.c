#include <time.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <signal.h>
#include <sys/wait.h>
#include <uv/errno.h>
#include "service.h"
#include "utils.h"

service_t service_create(const char * service_name, const char * program_path, const char ** argv, int argc, int flags) {
    syslog(LOG_INFO, "Creating service %s", service_name);
    syslog(LOG_INFO, "Program path %s", program_path);
    service_t service = get_empty_service();
    pid_t pid = fork();
    if (pid < 0) {
        syslog(LOG_ERR, "Failed to fork");
        return get_empty_service();
    }
    if (pid == 0) {
        if (access(program_path, F_OK | X_OK) != 0) {
            syslog(LOG_ERR, "Failed to access program %s", program_path);
            syslog(LOG_ERR, "%d %s", errno, strerror(errno));
            return get_empty_service();
        }

        syslog(LOG_INFO, "%s %s %s", "Starting service", service_name, program_path);
        execv(program_path, (char *const *) argv);
        syslog(LOG_ERR, "Failed to execv");
        syslog(LOG_ERR, "%d %s", errno, strerror(errno));
        return get_empty_service();
    }
    syslog(LOG_INFO, "Service %s started with pid %d", service_name, pid);
    service.pid = pid;
    service.start_time = time(NULL);
    service.service_name = service_name;
    service.formatted_service_name = strdup(format_service_name(service_name, pid, service.start_time));
    service.program_path = program_path;
    service.argv = argv;
    service.argc = argc;
    service.flags = flags;

    return service;
}

int service_close(service_t service) {
    syslog(LOG_INFO, "Closing service %s", service.service_name);
    if (service.service_name == NULL) {
        return -1;
    }

    kill(service.pid, SIGKILL);

    int status;
    waitpid(service.pid, &status, 0);
    syslog(LOG_INFO, "Service %s closed", service.service_name);
    return 0;
}

//service_t service_open(struct supervisor_t supervisor, const char * service_name) {
//    int i = get_free_service_index(supervisor);
//    if (i == -1) {
//        syslog(LOG_ERR, "No space for new service");
//        return get_empty_service();
//    } else {
//        // search for service_name in OS processes
//        // if found, add it to supervisor
//        // if not found, return empty service
//
//
//        return supervisor->services[i];
//    }
//}

service_t get_empty_service() {
    service_t empty_service;
    empty_service.service_name = NULL;
    empty_service.program_path = NULL;
    empty_service.argv = NULL;
    empty_service.argc = 0;
    empty_service.flags = 0;
    return empty_service;
}