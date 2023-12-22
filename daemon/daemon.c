#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>
#include <stdbool.h>
#include <getopt.h>
#include "utils.h"
#include "supervisor.h"
#include "signal_handler.h"
#include "constants.h"

#define SOCKET_PATH "/tmp/supervisor_daemon.sock"

typedef struct {
    int instance;
    int create_stopped;
    int restart_times;
} Options;

pthread_mutex_t service_status_mutex; // TODO: put in code
void daemonize();
void process_commands(int client_socket, char *response);
void parse_command_arguments(char *command_str, char *response_str);

int main() {
    struct sigaction sa;
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = handle_sigchld;
    sigemptyset(&sa.sa_mask);
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
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

    while (true) {
        client_socket = accept(server_socket, NULL, NULL);
        if (client_socket < 0) {
            perror("accept");
            continue;
        }
        char *response = malloc(256 * sizeof(char));
        process_commands(client_socket, response);
        write(client_socket, response, strlen(response));
        free(response);
        close(client_socket);
    }

    close(server_socket);
    unlink(SOCKET_PATH);
    syslog(LOG_NOTICE, "Supervisor daemon terminated");

    closelog();

    return 0;
}

void parse_command_arguments(char *command_str, char *response_str) {
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
        strcpy(response_str, "No command provided");
        return;
    }

    if (number_of_tokens == 1) {
        if (strcmp(command, "list-supervisors") == 0) {
            strcpy(response_str, "List supervisors");
            list_supervisors();
        } else {
            strcpy(response_str, "Unknown command");
            syslog(LOG_ERR, "Unknown command: %s", command);
            return;
        }
    } else {
        struct option long_options[] = {
                {"instance", required_argument, 0, 'i'},
                {"create-stopped", no_argument, 0, 'c'},
                {"restart-times", required_argument, 0, 'r'},
                {0, 0, 0, 0}
        };

        optind = 1;
        int option_index = 1;
        int c;

        while ((c = getopt_long(number_of_tokens, command_tokens, "i:cr:", long_options, &option_index)) != -1) {
            switch (c) {
                case 'i':
                    options.instance = atoi(optarg);
                    syslog(LOG_INFO, "Instance: %d", options.instance);
                    break;
                case 'c':
                    options.create_stopped = 1;
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
            strcpy(response_str, "Init");
            syslog(LOG_INFO, "Init supervisor %d", options.instance);
            supervisor_init(options.instance);
        } else if (strcmp(command, "list-supervisors") == 0) {
            strcpy(response_str, "List supervisors");
            list_supervisors();
        } else if (strcmp(command, "close") == 0) {
            strcpy(response_str, "Close");
            supervisor_close(supervisor_get(options.instance));
        } else if (strcmp(command, "create-service") == 0) {
            if (optind + 3 > number_of_tokens) {
                strcpy(response_str, "Not enough arguments for create-service");
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

            pid_t *new_pid = malloc(sizeof(pid_t));
            supervisor_create_service_wrapper(supervisor_get(options.instance), service_name, program_path,
                                              argv_service, argc_service, options.create_stopped |
                                                                          SUPERVISOR_FLAGS_RESTARTTIMES(
                                                                                  options.restart_times), new_pid);

            if (!new_pid) {
                strcpy(response_str, "Service not created");
            } else {
                sprintf(response_str, "Service created with pid %d\n", *new_pid);
            }
            free(new_pid);
            free(argv_service);
        } else if (strcmp(command, "open-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(response_str, "Not enough arguments for open-service");
                return;
            }

            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(response_str, "Invalid supervisor instance");
                return;
            }

            supervisor_open_service_wrapper(supervisor, pid);
            strcpy(response_str, "Service opened");
        } else if (strcmp(command, "close-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(response_str, "Not enough arguments for close-service");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(response_str, "Invalid supervisor instance");
                return;
            }

            supervisor_send_command_to_existing_service_wrapper(supervisor, pid, KILL_SERVICE);
            strcpy(response_str, "Service closed");
        } else if (strcmp(command, "remove-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(response_str, "Not enough arguments for remove-service");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(response_str, "Invalid supervisor instance");
                return;
            }

            supervisor_remove_service_wrapper(supervisor, pid);
            strcpy(response_str, "Service removed");
        } else if (strcmp(command, "suspend-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(response_str, "Not enough arguments for suspend-service");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(response_str, "Invalid supervisor instance");
                return;
            }

            // TODO: error handling with if's in the other calls
            if (supervisor_send_command_to_existing_service_wrapper(supervisor, pid, SUSPEND_SERVICE) == 0) {
                strcpy(response_str, "Service successfully suspended!");
            } else {
                strcpy(response_str, "Encountered error while suspending service!");
            }
        } else if (strcmp(command, "service-status") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(response_str, "Not enough arguments for service-status");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(response_str, "Invalid supervisor instance");
                return;
            }
            int status = supervisor_send_command_to_existing_service_wrapper(supervisor, pid, STATUS_SERVICE);
            append_service_status_to_string(status, response_str);
        } else if (strcmp(command, "resume-service") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(response_str, "Not enough arguments for resume-service");
                return;
            }
            pid_t pid = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(response_str, "Invalid supervisor instance");
                return;
            }

            if (supervisor_send_command_to_existing_service_wrapper(supervisor, pid, RESUME_SERVICE) == 0) {
                strcpy(response_str, "Service successfully resumed!");
            } else {
                strcpy(response_str, "Encountered error while resuming service!");
            }
        } else if (strcmp(command, "list-supervisor") == 0) {
            unsigned int *count = malloc(sizeof(unsigned int));
            const char ***service_names = malloc(sizeof(char **));
            supervisor_t *supervisor = supervisor_get(options.instance);
            if (!supervisor) {
                strcpy(response_str, "Invalid supervisor instance");
                return;
            }
            supervisor_list(supervisor, service_names, count);
            strcpy(response_str, "List supervisor\n");
            for (int i = 0; i < *count; i++) {
                const char *service_name = (*service_names)[i];
                int service_index = get_service_index_from_service_name(supervisor, service_name);
                strcat(response_str, (*service_names)[i]);
                strcat(response_str, "  -  ");
                int status = supervisor_send_command_to_existing_service_wrapper(supervisor, supervisor->services[service_index].pid, STATUS_SERVICE);
                append_service_status_to_string(status, response_str);
                strcat(response_str, "\n");
            }
            syslog(LOG_INFO, "list: %s", response_str);
            for (int i = 0; i < *count; i++) {
                free((void *) (*service_names)[i]);
            }
            free(*service_names);
            free(count);
        } else if (strcmp(command, "supervisor-freelist") == 0) {
            if (optind + 1 > number_of_tokens) {
                strcpy(response_str, "Not enough arguments for supervisor-freelist");
                return;
            }

            supervisor_t* supervisor = supervisor_get(options.instance);
            if (supervisor == NULL) {
                strcpy(response_str, "Invalid supervisor instance");
                return;
            }

            int count = atoi(command_tokens[optind++]);
            pid_t* pid_array = malloc(count * sizeof(pid_t));
            for (int i = 0; i < count; i++) {
                pid_array[i] = atoi(command_tokens[optind++]);
            }

            for (int i = 0; i < count; ++i) {
                syslog(LOG_INFO, "pid_array[%d] = %d", i, pid_array[i]);
            }

            supervisor_freelist(supervisor, pid_array, count);
            strcpy(response_str, "Services freed");
        } else {
            strcpy(response_str, "Unknown command");
            syslog(LOG_ERR, "Unknown command: %s", command);
            return;
        }
    }

    for (int i = 0; i < 64; i++) {
        free(command_tokens[i]);
    }
}

void process_commands(int client_socket, char *response) {
    char buffer[256];
    int len = read(client_socket, buffer, sizeof(buffer) - 1);
    if (len == 0) {
        return;
    }
    buffer[len] = '\0';

    parse_command_arguments(buffer, response);

    syslog(LOG_INFO, "Received command: %s", buffer);
    syslog(LOG_INFO, "Response: %s", response);
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