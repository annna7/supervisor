#include <fcntl.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <sys/syslog.h>
#include "signal_handler.h"
#include "supervisor.h"
#include "constants.h"
#include "global_state.h"
#include <unistd.h>
#include <uv/errno.h>
#include <stdio.h>

// TODO: verify that child service was not removed from supervisor monitoring and proper exception handling
void handle_sigchld(int sig, siginfo_t *siginfo, void *context) {
    pid_t pid = siginfo->si_pid;

    char status_message[64];

    int status;
    if (siginfo->si_code == CLD_STOPPED) {
        snprintf(status_message, sizeof(status_message), "STOPPED %d", siginfo->si_pid);
    } else if (siginfo->si_code == CLD_CONTINUED) {
        snprintf(status_message, sizeof(status_message), "CONTINUED %d", siginfo->si_pid);
    } else if (siginfo->si_code == CLD_EXITED || siginfo->si_code == CLD_KILLED || siginfo->si_code == CLD_DUMPED) {
        while ((pid = waitpid(-1, &status, WNOHANG)) > 0) {
            if (WIFEXITED(status)) {
                snprintf(status_message, sizeof(status_message), "TERMINATED %d %d", siginfo->si_pid, WEXITSTATUS(status));
            } else if (WIFSIGNALED(status)) {
                if (WTERMSIG(status) == SIGSEGV || WTERMSIG(status) == SIGBUS) {
                    snprintf(status_message, sizeof(status_message), "CRASHED %d", siginfo->si_pid);
                } else {
                    snprintf(status_message, sizeof(status_message), "TERMINATED %d", siginfo->si_pid);
                }
            } else {
                // finer handling here?
                snprintf(status_message, sizeof(status_message), "TERMINATED %d", siginfo->si_pid);
            }
        }
    }

    if (write(pipe_fd[1], status_message, strlen(status_message) + 1) == -1) {
        syslog(LOG_ERR, "Failed to write to pipe");
        syslog(LOG_ERR, "Error: %s %d", strerror(errno), errno);
    }
}


void handle_sigterm(int sig, siginfo_t *siginfo, void *context) {
    syslog(LOG_INFO, "Received SIGTERM AND STOPPED LOOP");
    keep_running = 0;
}

void handle_sigchild_status(char *status, pid_t pid) {
    int supervisor_instance = get_supervisor_instance_from_service_pid(pid);
    if (supervisor_instance == -1) {
        strcat(global_response_str, "INVALID SUPERVISOR IN HANDLE SIGCHILD");
        return;
    }

    supervisor_t *supervisor = supervisor_get(supervisor_instance);
    if (supervisor == NULL) {
        strcat(global_response_str, "INVALID SUPERVISOR IN HANDLE SIGCHILD");
        return;
    }

    int service_index = get_service_index_from_pid(supervisor, pid);
    if (service_index == -1) {
        strcat(global_response_str, "INVALID SERVICE IN HANDLE SIGCHILD");
        return;
    }

    pthread_mutex_lock(&status_mutex);
    if (strcmp(status, "STOPPED") == 0) {
        supervisor->services[service_index].status = SUPERVISOR_STATUS_STOPPED;
        strcat(global_response_str, "STOPPED ");
        strcat(global_response_str, supervisor->services[service_index].formatted_service_name);
        strcat(global_response_str, " ");
        pthread_mutex_unlock(&status_mutex);
    } else if (strcmp(status, "TERMINATED") == 0) {
        supervisor->services[service_index].status = SUPERVISOR_STATUS_TERMINATED;
        strcat(global_response_str, "TERMINATED ");
        strcat(global_response_str, supervisor->services[service_index].formatted_service_name);
        strcat(global_response_str, " ");
        pthread_mutex_unlock(&status_mutex);
    } else if (strcmp(status, "CRASHED") == 0) {
        supervisor->services[service_index].status = SUPERVISOR_STATUS_CRASHED;
        strcat(global_response_str, "CRASHED ");
        strcat(global_response_str, supervisor->services[service_index].formatted_service_name);
        strcat(global_response_str, " ");
        pthread_mutex_unlock(&status_mutex);
        if (supervisor->services[service_index].restart_times_left > 0) {
            syslog(LOG_INFO, "Restarting service %s %s", supervisor->services[service_index].formatted_service_name, supervisor->services[service_index].program_path);
            service_restart(&supervisor->services[service_index]);
        } else {
            syslog(LOG_INFO, "Service %s has no restart times left", supervisor->services[service_index].formatted_service_name);
        }
    } else if (strcmp(status, "CONTINUED") == 0) {
        supervisor->services[service_index].status = SUPERVISOR_STATUS_RUNNING;
        strcat(global_response_str, "CONTINUED ");
        strcat(global_response_str, supervisor->services[service_index].formatted_service_name);
        strcat(global_response_str, " ");
        pthread_mutex_unlock(&status_mutex);
    } else {
        strcat(global_response_str, "INVALID STATUS IN HANDLE SIGCHILD");
        pthread_mutex_unlock(&status_mutex);
        return;
    }
}

void* sigchild_listener(void *arg) {
    syslog(LOG_INFO, "Starting sigchild listener");
    char buf[128];

    while (keep_running) {
        ssize_t nbytes = read(pipe_fd[0], buf, sizeof(buf));
        if (nbytes > 0) {
            syslog(LOG_INFO, "Received sigchild buffer size: %s", buf);
            char *status = strtok(buf, " ");
            char *pid = strtok(NULL, " ");
            syslog(LOG_INFO, "Received sigchild status: %s %s", status, pid);
            if (status != NULL && pid != NULL) {
                handle_sigchild_status(status, atoi(pid));
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
}
