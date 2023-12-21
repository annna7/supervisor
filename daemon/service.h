#ifndef SERVICE_SERVICE_H
#define SERVICE_SERVICE_H

#include <stdbool.h>

typedef struct {
    pid_t pid;
    time_t start_time;
    const char * service_name;
    const char * formatted_service_name;
    const char * program_path;
    const char ** argv;
    int restart_times_left;
    int argc;
    int status;
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
// KILLED - disappeared from OS pid's (SIGKILL) ???
int service_status(service_t* service);
// for STOPPED services - resume their execution
// for PENDING services - start them
int service_resume(service_t* service);
// for PENDING services - cancel the start
int service_cancel(service_t* service);
// RUNNING -> STOPPED
int service_suspend(service_t* service);
#endif