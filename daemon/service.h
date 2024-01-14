#ifndef SERVICE_SERVICE_H
#define SERVICE_SERVICE_H

#include <stdbool.h>
#include "constants.h"

typedef struct {
    pid_t pid;
    time_t start_time;
    char service_name[MAX_SERVICE_NAME_LENGTH];
    char program_path[MAX_PROGRAM_PATH_LENGTH];
    char formatted_service_name[MAX_FORMATTED_SERVICE_NAME_LENGTH];
    char *argv[MAX_ARGS];
    int argc;
    int status;
    int restart_times_left;
} service_t;

service_t get_empty_service();
service_t service_create(const char * service_name, const char * program_path, const char ** argv, int argc, int flags, time_t start_time);
service_t service_open(pid_t pid);
// kill service (SIGKILL, doesn't appear in OS pids anymore)
// TODO Q: should killed services be automatically removed from supervisor?
int service_kill(service_t* service);
// remove service from supervisor
// RUNNING - the OS can schedule it
// PENDING - hasn't started yet (potentially add scheduling option)
// STOPPED - the OS can't schedule it, but can be resumed (service_suspend, service_resume)
// TERMINATED - disappeared from OS pid's (SIGKILL) ???
// CRASHED - terminated badly but wasn't restarted
int service_status(service_t* service);
// for STOPPED services - resume their execution
// for PENDING services - start them
int service_resume(service_t* service);
// for PENDING services - cancel the start
int service_cancel(service_t* service);
// RUNNING -> STOPPED
int service_suspend(service_t* service);
int service_restart(service_t* service);
#endif