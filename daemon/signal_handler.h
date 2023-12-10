#ifndef SUPERVISOR_SIGNAL_HANDLER_H
#define SUPERVISOR_SIGNAL_HANDLER_H

#include <bits/types/siginfo_t.h>

void handle_sigchld(int sig, siginfo_t *siginfo, void *context);

#endif //SUPERVISOR_SIGNAL_HANDLER_H
