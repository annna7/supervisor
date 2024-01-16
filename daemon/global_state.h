#ifndef SUPERVISOR_GLOBAL_STATE_H
#define SUPERVISOR_GLOBAL_STATE_H
#include <bits/types/sig_atomic_t.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "pthread.h"
#include "constants.h"

extern char global_response_str[];
extern volatile sig_atomic_t keep_running;
extern int pipe_fd[];
extern int scheduling_pipe_fd[];
extern pthread_mutex_t status_mutex;
void append_to_global_response_str(const char *format, ...);
#endif
