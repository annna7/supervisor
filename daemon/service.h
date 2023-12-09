#ifndef SERVICE_SERVICE_H
#define SERVICE_SERVICE_H

#define SUPERVISOR_FLAGS_CREATESTOPPED 0x1
#define SUPERVISOR_FLAGS_RESTARTTIMES(times) ((times & 0xF) << 16)
#define SUPERVISOR_STATUS_RUNNING 0x1
#define SUPERVISOR_STATUS_PENDING 0x2
#define SUPERVISOR_STATUS_STOPPED 0x4

typedef struct {
    pid_t pid;
    time_t start_time;
    const char * service_name;
    const char * formatted_service_name;
    const char * program_path;
    const char ** argv;
    int argc;
    int flags;
    int status;
} service_t;

struct supervisor_t;

service_t get_empty_service();
service_t service_create(const char * service_name, const char * program_path, const char ** argv, int argc, int flags);
int service_close(service_t service);
service_t service_open(const char * service_name);
int service_status(service_t service);
int service_suspend(service_t service);
int service_resume(service_t service);
int service_cancel(service_t service);
int service_remove(service_t service);

#endif