#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>
#include <getopt.h>
#include <pthread.h>
#include "utils.h"
#include "supervisor.h"
#include "signal_handler.h"
#include "constants.h"
#include "global_state.h"
#include "existing_process_handler.h"

#define SOCKET_PATH "/tmp/supervisor_daemon.sock"

typedef struct {
    int instance;
    int create_stopped;
    int restart_times;
} Options;


void daemonize();
void process_commands(int client_socket);
void parse_command_arguments(char *command_str);

int main() {
    struct sigaction sa_sig_term;
    sa_sig_term.sa_flags = SA_SIGINFO;
    sa_sig_term.sa_sigaction = handle_sigterm;
    sigemptyset(&sa_sig_term.sa_mask);

    if (sigaction(SIGTERM, &sa_sig_term, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    if (sigaction(SIGINT, &sa_sig_term, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    struct sigaction sa_sig_child;
    sa_sig_child.sa_flags = SA_SIGINFO;
    sa_sig_child.sa_sigaction = handle_sigchld;
    sigemptyset(&sa_sig_child.sa_mask);
    if (sigaction(SIGCHLD, &sa_sig_child, NULL) == -1) {
        perror("sigaction");
        return 1;
    }

    openlog("supervisor", LOG_PID, LOG_DAEMON);
    syslog(LOG_NOTICE, "Supervisor daemon starting");
    daemonize();

    int server_socket, client_socket;
    struct sockaddr_un server_addr;

    server_socket = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&server_addr, 0, sizeof(struct sockaddr_un));
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    unlink(SOCKET_PATH);

    if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(struct sockaddr_un)) < 0) {
        perror("bind");
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 5) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }

    syslog(LOG_NOTICE, "Supervisor daemon started");

    // setting up the pipe for communication with the async signal handler
    if (pipe(pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }

    if (fcntl(pipe_fd[1], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    if (fcntl(pipe_fd[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    //creating pipe for communication between scheduled services and daemon
    if (pipe(scheduling_pipe_fd) == -1) {
        perror("pipe");
        exit(EXIT_FAILURE);
    }
    if (fcntl(scheduling_pipe_fd[1], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }
    if (fcntl(scheduling_pipe_fd[0], F_SETFL, O_NONBLOCK) == -1) {
        perror("fcntl");
        exit(EXIT_FAILURE);
    }

    pthread_mutex_init(&status_mutex, NULL);

    pthread_t scheduling_thread;

    if (pthread_create(&scheduling_thread, NULL, scheduling_thread_function, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    pthread_t signal_handler_thread;
    if (pthread_create(&signal_handler_thread, NULL, sigchild_listener, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    syslog(LOG_INFO, "created signal handler thread");

    pthread_t service_polling_thread;
    if (pthread_create(&service_polling_thread, NULL, service_polling_thread_function, NULL) != 0) {
        perror("pthread_create");
        exit(EXIT_FAILURE);
    }

    while (keep_running) {
        client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        global_response_str[0] = '\0';
        process_commands(client_socket);
        write(client_socket, global_response_str, strlen(global_response_str));
        global_response_str[0] = '\0';
        close(client_socket);
    }

    pthread_join(signal_handler_thread, NULL);
    pthread_join(service_polling_thread, NULL);

    close(server_socket);
    unlink(SOCKET_PATH);
    syslog(LOG_NOTICE, "Supervisor daemon terminated");

    closelog();

    return 0;
}

void parse_command_arguments(char *command_str) {
    int number_of_tokens;
    char *command_tokens[64];
    for (int i = 0; i < 64; i++) {
        command_tokens[i] = malloc(64 * sizeof(char));
    }

    parse_string(command_str, &number_of_tokens, command_tokens);

    Options options = {0, 0, 0};

    char *command = NULL;
    if (number_of_tokens > 0) {
        command = command_tokens[0];
        optind++;
    }

    if (command == NULL) {
        strcpy(global_response_str, "No command provided");
        return;
    }

    if (number_of_tokens == 1) {
        if (strcmp(command, "get-response") == 0) {
            return;
        }
        if (strcmp(command, "list-supervisors") == 0) {
            strcpy(global_response_str, "Supervisors:\n");
            list_supervisors();
        } else {
            strcpy(global_response_str, "Unknown command");
            syslog(LOG_ERR, "Unknown command: %s", command);
        }
    } else {
        struct option long_options[] = {
                {"instance", required_argument, 0, 'i'},
                {"create-stopped", optional_argument, 0, 'c'},
                {"restart-times", required_argument, 0, 'r'},
                {0, 0, 0, 0}
        };

        optind = 1;
        int option_index = 1;
        int c;

        while ((c = getopt_long(number_of_tokens, command_tokens, "i:c::r:", long_options, &option_index)) != -1) {
            switch (c) {
                case 'i':
                    options.instance = atoi(optarg);
                    syslog(LOG_INFO, "Instance: %d", options.instance);
                    break;
                case 'c':
                    if(optarg == NULL && optind < number_of_tokens && command_tokens[optind][0] != '-'){
                        optarg = command_tokens[optind++];
                    }
                    if(optarg != NULL){
                        options.create_stopped = atoi(optarg) + 1;
                        syslog(LOG_INFO, "Optional Argument: %d", options.create_stopped);

                    }
                    else{
                        options.create_stopped = 1;
                        syslog(LOG_INFO, "Default Argument: %d", options.create_stopped);
                    }
                    break;
                case 'r':
                    options.restart_times = atoi(optarg);
                    break;
                case '?':
                    break;
                default:
                    syslog(LOG_ERR,"?? getopt returned character code 0%o ??\n", c);
                    break;
            }
        }

        if (strcmp(command, "init") == 0) {
            sprintf(global_response_str, "Initialized supervisor instance %d", options.instance);
            syslog(LOG_INFO, "Init supervisor %d", options.instance);
            supervisor_init(options.instance);
        } else if (strcmp(command, "close") == 0) {
            supervisor_close(supervisor_get(options.instance));
        } else if (strcmp(command, "create-service") == 0) {
            if (optind + 3 > number_of_tokens) {
                strcpy(global_response_str, "Not enough arguments for create-service");
                return;
            }

            char *program_path = command_tokens[optind++];
            char *service_name = command_tokens[optind++];
            int argc_service = atoi(command_tokens[optind++]);

            char **argv_service = malloc(
                    (argc_service + 2) * sizeof(char *)); // +2 for the program name and the NULL terminator

            argv_service[0] = service_name;

            for (int i = 0; i < argc_service; i++) {
                argv_service[i + 1] = command_tokens[optind++];
            }

            argv_service[argc_service + 1] = NULL;

            syslog(LOG_INFO, "Option restart %d %d", options.restart_times, SUPERVISOR_FLAGS_RESTARTTIMES(options.restart_times));
            pid_t *new_pid = malloc(sizeof(pid_t));
            supervisor_create_service_wrapper(supervisor_get(options.instance), service_name, program_path,
                                              argv_service, argc_service, options.create_stopped |
                                                                          SUPERVISOR_FLAGS_RESTARTTIMES(
                                                                                  options.restart_times), new_pid);

            if (!new_pid) {
                strcpy(global_response_str, "Service not created");
            } else {
                sprintf(global_response_str, "Service created with pid %d\n", *new_pid);
            }
            free(new_pid);
            free(argv_service);
        } else if (strcmp(command, "open-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(global_response_str, "Not enough arguments for open-service");
                return;
            }

            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(global_response_str, "Invalid supervisor instance");
                return;
            }

            supervisor_open_service_wrapper(supervisor, pid);
            strcpy(global_response_str, "Service opened");
        } else if (strcmp(command, "close-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(global_response_str, "Not enough arguments for close-service");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(global_response_str, "Invalid supervisor instance");
                return;
            }

            supervisor_send_command_to_existing_service_wrapper(supervisor, pid, KILL_SERVICE);
            strcpy(global_response_str, "Service closed");
        } else if (strcmp(command, "remove-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(global_response_str, "Not enough arguments for remove-service");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(global_response_str, "Invalid supervisor instance");
                return;
            }

            supervisor_remove_service_wrapper(supervisor, pid);
            strcpy(global_response_str, "Service removed");
        } else if (strcmp(command, "suspend-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(global_response_str, "Not enough arguments for suspend-service");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(global_response_str, "Invalid supervisor instance");
                return;
            }

            // TODO: error handling with if's in the other calls
            if (supervisor_send_command_to_existing_service_wrapper(supervisor, pid, SUSPEND_SERVICE) == 0) {
                strcpy(global_response_str, "Service successfully suspended!");
            } else {
                strcpy(global_response_str, "Encountered error while suspending service!");
            }
        } else if (strcmp(command, "service-status") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(global_response_str, "Not enough arguments for service-status");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(global_response_str, "Invalid supervisor instance");
                return;
            }
            int status = supervisor_send_command_to_existing_service_wrapper(supervisor, pid, STATUS_SERVICE);
            append_service_status_to_string(status, global_response_str);
        } else if (strcmp(command, "resume-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(global_response_str, "Not enough arguments for resume-service");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(global_response_str, "Invalid supervisor instance");
                return;
            }

            if (supervisor_send_command_to_existing_service_wrapper(supervisor, pid, RESUME_SERVICE) == 0) {
                strcpy(global_response_str, "Service successfully resumed!");
            } else {
                strcpy(global_response_str, "Encountered error while resuming service!");
            }
        } else if (strcmp(command, "list-supervisor") == 0) {
            unsigned int *count = malloc(sizeof(unsigned int));
            const char ***service_names = malloc(sizeof(char **));
            supervisor_t *supervisor = supervisor_get(options.instance);
            if (!supervisor) {
                strcpy(global_response_str, "Invalid supervisor instance");
                return;
            }
            supervisor_list(supervisor, service_names, count);
            strcpy(global_response_str, "List supervisor\n");
            for (int i = 0; i < *count; i++) {
                const char *service_name = (*service_names)[i];
                int service_index = get_service_index_from_service_name(supervisor, service_name);
                strcat(global_response_str, (*service_names)[i]);
                strcat(global_response_str, "  -  ");
                int status = supervisor_send_command_to_existing_service_wrapper(supervisor, supervisor->services[service_index].pid, STATUS_SERVICE);
                append_service_status_to_string(status, global_response_str);
                strcat(global_response_str, "\n");
            }
            for (int i = 0; i < *count; i++) {
                free((void *) (*service_names)[i]);
            }
            free(*service_names);
            free(count);
        } else if (strcmp(command, "supervisor-freelist") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(global_response_str, "Not enough arguments for supervisor-freelist");
                return;
            }

            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(global_response_str, "Invalid supervisor instance");
                return;
            }

            int count = atoi(command_tokens[optind++]);
            pid_t* pid_array = malloc(count * sizeof(pid_t));
            for (int i = 0; i < count; i++) {
                pid_array[i] = atoi(command_tokens[optind++]);
            }

            supervisor_freelist(supervisor, pid_array, count);
            strcpy(global_response_str, "Services freed");
        } else {
            strcpy(global_response_str, "Unknown command");
            syslog(LOG_ERR, "Unknown command: %s", command);
        }
    }

    for (int i = 0; i < 64; i++) {
        free(command_tokens[i]);
    }
}

void process_commands(int client_socket) {
    char buffer[256];
    int len = read(client_socket, buffer, sizeof(buffer) - 1);
    if (len == 0) {
        return;
    }
    buffer[len] = '\0';

    parse_command_arguments(buffer);

    syslog(LOG_INFO, "Received command: %s", buffer);
    syslog(LOG_INFO, "Response: %s", global_response_str);
}

void daemonize() {
    pid_t pid;

    pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }

//    signal(SIGCHLD, SIG_IGN);
//    signal(SIGHUP, SIG_IGN);

    pid = fork();

    if (pid < 0) {
        exit(EXIT_FAILURE);
    }

    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }

    umask(0);

    chdir("/");

    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        close(x);
    }
}