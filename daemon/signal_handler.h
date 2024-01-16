#ifndef SUPERVISOR_SIGNAL_HANDLER_H
#define SUPERVISOR_SIGNAL_HANDLER_H

#include <bits/types/siginfo_t.h>

// these are the actual signal handlers for SIGCHLD and SIGTERM/SIGINT
void handle_sigchld(int sig, siginfo_t *siginfo, void *context);
void handle_sigterm(int sig, siginfo_t *siginfo, void *context);

// this is called (actually listens via pipe) when something happens to a child process (so we can use whatever functions we need)
void* sigchild_listener(void *arg);

// Used for updating service state from PENDING to RUNNING
void* scheduling_thread_function(void * args);
#endif //SUPERVISOR_SIGNAL_HANDLER_H
