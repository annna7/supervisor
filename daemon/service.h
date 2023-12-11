#ifndef SERVICE_SERVICE_H
#define SERVICE_SERVICE_H

#include <stdbool.h>

#define SUPERVISOR_FLAGS_CREATESTOPPED 0x1
#define SUPERVISOR_FLAGS_RESTARTTIMES(times) ((times & 0xF) << 16)
#define SUPERVISOR_STATUS_RUNNING 0x1
#define SUPERVISOR_STATUS_PENDING 0x2
#define SUPERVISOR_STATUS_STOPPED 0x4
#define SUPERVISOR_STATUS_KILLED 0x8

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
// TODO: should killed services be automatically removed from supervisor?
int service_kill(service_t service);
// remove service from supervisor
// RUNNING - the OS can schedule it
// PENDING - hasn't started yet (potentially add scheduling option)
// STOPPED - the OS can't schedule it, but can be resumed (service_suspend, service_resume)
// KILLED - disappeared from OS pid's (SIGKILL) ???
int service_status(service_t service);
// for STOPPED services - resume their execution
// for PENDING services - start them
int service_resume(service_t service);
// for PENDING services - cancel the start
int service_cancel(service_t service);
// RUNNING -> STOPPED
int service_suspend(service_t service);

#endif