#include "global_state.h"

char global_response_str[RESPONSE_STR_SIZE];
// we have a while (true) in main, but we still need to join threads etc. when the daemon is terminated
// so, we use a global variable to control when termination happens and this way we can do the cleanup
volatile sig_atomic_t keep_running = 1;
pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;
// pipe for communication between signal handler and main thread
int pipe_fd[2];