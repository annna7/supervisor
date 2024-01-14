#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include "signal_handler.h"
#include "supervisor.h"
#include "constants.h"
#include "utils.h"
#include "global_state.h"
#include <unistd.h>

// TODO: verify that child service was not removed from supervisor monitoring and proper exception handling
// TODO: how to show logs? create global string (response_str?)
// TODO: also malloc is not async-signal-safe
void handle_sigchld(int sig, siginfo_t *siginfo, void *context) {
    pid_t pid = siginfo->si_pid;
//    write(pipe_fd[1], "PROCESS", 10);

    int supervisor_instance = get_supervisor_instance_from_service_pid(pid);
    if (supervisor_instance == -1) {
        write(pipe_fd[1], "INVALID", 10);
        return;
    }

    supervisor_t *supervisor = supervisor_get(supervisor_instance);
    if (supervisor == NULL) {
        write(pipe_fd[1], "INVALID", 10);
        return;
    }

    int service_index = get_service_index_from_pid(supervisor, pid);
    if (service_index == -1) {
        write(pipe_fd[1], "INVALID", 10);
        return;
    }

    int status;
    if (siginfo->si_code == CLD_STOPPED) {
        write(pipe_fd[1], "STOPPED", 7);
    } else if (siginfo->si_code == CLD_CONTINUED) {
        write(pipe_fd[1], "CONTINUED", 9);
    } else if (siginfo->si_code == CLD_EXITED || siginfo->si_code == CLD_KILLED || siginfo->si_code == CLD_DUMPED) {
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (WIFEXITED(status)) {
                write(pipe_fd[1], "EXITED", 6);
            } else if (WIFSIGNALED(status)) {
                if (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS) {
                    write(pipe_fd[1], "CRASHED", 8);
                } else if (WTERMSIG(status) == SIGTERM) {
                    write(pipe_fd[1], "SIGNALED", 8);
                }
            }
        }
    }
}



//    syslog(LOG_INFO, "Received SIGCHLD from %d", pid);

//     int status;

//     while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
//         int instance = get_supervisor_instance_from_service_pid(pid);
//         supervisor_t *supervisor = supervisor_get(instance);
//         if (supervisor == NULL) {
// //        syslog(LOG_ERR, "Invalid supervisor instance");
//             return;
//         }

//         int service_index = get_service_index_from_pid(supervisor, pid);
//         if (service_index == -1) {
// //        syslog(LOG_ERR, "Invalid service, logic error occurred");
//             return;
//         }

//         // TODO: Why kill -SIGCONT <pid> isn't working??
// //    syslog(LOG_INFO, "Service from instance %d, index %d", supervisor->instance, service_index);
//         if (WIFCONTINUED(status) || (WIFSIGNALED(status) && (WTERMSIG(status) == SIGCONT))) {
//             write(pipe_fd[1], "CONTINUED", 9);
//            // syslog(LOG_INFO, "Child %d continued", pid);
//             supervisor->services[service_index].status = SUPERVISOR_STATUS_RUNNING;
//         }
//         else if (WIFSTOPPED(status)) {
// //        syslog(LOG_INFO, "Child %d stopped with signal %d", pid, WSTOPSIG(status));
//             write(pipe_fd[1], "STOPPED", 7);
//             supervisor->services[service_index].status = SUPERVISOR_STATUS_STOPPED;
//         } else if (WIFEXITED(status)) {
// //        syslog(LOG_INFO, "Child %d exited normally with status %d", pid, WEXITSTATUS(status));
// //        supervisor_remove_service_wrapper(supervisor, pid);
// //        supervisor_send_command_to_existing_service_wrapper(supervisor, pid, REMOVE_SERVICE);
//             write(pipe_fd[1], "EXITED", 6);
//             supervisor->services[service_index].status = SUPERVISOR_STATUS_KILLED;
//         } else if (WIFSIGNALED(status) && (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS)) {
//             write(pipe_fd[1], "CRASHED", 8);
// //            service_t *service_clone = malloc(sizeof(service_t));
// //            *service_clone = supervisor->services[service_index];
// //          syslog(LOG_INFO, "Child %d crashed with %d", pid, WTERMSIG(status));
//             int restart_times_left = supervisor->services[service_index].restart_times_left;
//             if (restart_times_left == 0) {
// //                syslog(LOG_INFO, "No restarts left for service %s", service_clone->service_name);
//                 return;
//             }
// //            syslog(LOG_INFO, "Restarting service %s", service_clone->service_name);
// //            pid_t *new_pid = malloc(sizeof(pid_t));
// //            supervisor[instance].services[service_index].status = SUPERVISOR_STATUS_PENDING;
// //            supervisor[instance].services[service_index].restart_times_left = restart_times_left - 1;
// //            supervisor[instance].services[service_index].pid = new_pid;
// //            free(new_pid);
// //            free(service_clone);
//         } else {
// //            syslog(LOG_INFO, "Child %d terminated with signal %d", pid, WTERMSIG(status));
//             supervisor->services[service_index].status = -1;
//         }
//     }

void handle_sigterm(int sig, siginfo_t *siginfo, void *context) {
    keep_running = 0;
}

void* sigchild_listener(void *arg) {
    syslog(LOG_INFO, "Starting sigchild listener");
    char buf[10];

    while (keep_running) {
        ssize_t nbytes = read(pipe_fd[0], buf, sizeof(buf));
        if (nbytes > 0) {
            syslog(LOG_INFO, "Received %s", buf);
            strncat(global_response_str, buf, nbytes);
        }
        sleep(1);
    }
}