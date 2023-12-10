#include <fcntl.h>
#include <sys/syslog.h>
#include <sys/wait.h>
#include <stdlib.h>
#include "signal_handler.h"
#include "supervisor.h"

void handle_sigchld(int sig, siginfo_t *siginfo, void *context) {
    pid_t pid = siginfo->si_pid;
    syslog(LOG_INFO, "Received SIGCHLD from %d", pid);

    int status;
    waitpid(pid, &status, 0);

    int instance = get_supervisor_instance_from_service_pid(pid);
    supervisor_t* supervisor = supervisor_get(instance);
    if (supervisor == NULL) {
        syslog(LOG_ERR, "Invalid supervisor instance");
        return;
    }

    int service_index = get_service_index_from_pid(supervisor, pid);
    if (service_index == -1) {
        syslog(LOG_ERR, "Invalid service, logic error occurred");
        return;
    }

    if (WIFEXITED(status)) {
        syslog(LOG_INFO, "Child %d exited normally with status %d", pid, WEXITSTATUS(status));
        supervisor_remove_service_wrapper(supervisor, pid);
    } else if (WIFSIGNALED(status)) {
        syslog(LOG_INFO, "Child %d exited with signal %d", pid, WTERMSIG(status));
        service_t* service_clone = malloc(sizeof(service_t));
        *service_clone = supervisor->services[service_index];
        supervisor_remove_service_wrapper(supervisor, pid);
        if (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS) {
            syslog(LOG_INFO, "Child %d crashed", pid, WTERMSIG(status));
            int restart_times_left = supervisor->services[service_index].restart_times_left;
            if (restart_times_left == 0) {
                syslog(LOG_INFO, "No restarts left for service %s", service_clone->service_name);
                return;
            }
            syslog(LOG_INFO, "Restarting service %s", service_clone->service_name);
            pid_t* new_pid = malloc(sizeof(pid_t));
            supervisor_create_service_wrapper(supervisor, service_clone->service_name, service_clone->program_path,
                                              service_clone->argv, service_clone->argc, service_clone->restart_times_left - 1, new_pid);
            syslog(LOG_INFO, "New pid %d", *new_pid);
            free(new_pid);
            free(service_clone);
        } else {
            syslog(LOG_INFO, "Child %d terminated with signal %d", pid, WTERMSIG(status));
        }
    } else if (WIFSTOPPED(status)) {
        syslog(LOG_INFO, "Child %d stopped with signal %d", pid, WSTOPSIG(status));
        supervisor->services[service_index].status = SUPERVISOR_STATUS_STOPPED;
    } else if (WIFCONTINUED(status)) {
        syslog(LOG_INFO, "Child %d continued", pid);
        supervisor->services[service_index].status = SUPERVISOR_STATUS_RUNNING;
    } else {
        syslog(LOG_INFO, "Child %d exited with unknown status %d", pid, status);
        supervisor_remove_service_wrapper(supervisor, pid);
    }
}
