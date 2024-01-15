#include "global_state.h"

char global_response_str[RESPONSE_STR_SIZE] = {0};
// we have a while (true) in main, but we still need to join threads etc. when the daemon is terminated
// so, we use a global variable to control when termination happens and this way we can do the cleanup
volatile sig_atomic_t keep_running = 1;
pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;
// pipe for communication between signal handler and main thread
int pipe_fd[2];
char buffer[RESPONSE_STR_SIZE];

void append_to_global_response_str(const char *format, ...) {
    va_list args;
    va_start(args, format);
    vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);

    int currentLength = strlen(global_response_str);
    int availableSpace = sizeof(global_response_str) - currentLength - 1; // -1 for the null terminator

    strncat(global_response_str, buffer, availableSpace);
}
