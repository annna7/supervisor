#ifndef SUPERVISOR_GLOBAL_STATE_H
#define SUPERVISOR_GLOBAL_STATE_H
#include "constants.h"
#include <bits/types/sig_atomic_t.h>
#include "pthread.h"

extern char global_response_str[];
extern volatile sig_atomic_t keep_running;
extern int pipe_fd[];
extern pthread_mutex_t status_mutex;

#endif
