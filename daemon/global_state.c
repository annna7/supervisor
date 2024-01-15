#include "global_state.h"

char global_response_str[RESPONSE_STR_SIZE];
volatile sig_atomic_t keep_running = 1;
pthread_mutex_t status_mutex = PTHREAD_MUTEX_INITIALIZER;
int pipe_fd[2];