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

#define SOCKET_PATH "/tmp/supervisor_daemon.sock"

typedef struct {
    int instance;
    int create_stopped;
    int restart_times;
} Options;

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

            char *service_name = command_tokens[optind++];
            char *program_path = command_tokens[optind++];
            int argc_service = atoi(command_tokens[optind++]);

            char **argv_service = malloc((argc_service + 2) * sizeof(char*)); // +2 for the program name and the NULL terminator

            argv_service[0] = service_name;

            for(int i = 0; i < argc_service; i++) {
                argv_service[i+1] = command_tokens[optind++];
            }

            argv_service[argc_service + 1] = NULL;

            supervisor_create_service_wrapper(supervisor_get(options.instance), service_name, program_path,
                                              argv_service, argc_service, options.create_stopped | SUPERVISOR_FLAGS_RESTARTTIMES(options.restart_times));

            free(argv_service);
//        } else if (strcmp(command, "open-service") == 0) {
//            if (optind + 2 > number_of_tokens) {
//                strcpy(response_str, "Not enough arguments for open-service");
//                return;
//            }
//
//            pid_t pid = atoi(command_tokens[optind++]);
//            int instance = atoi(command_tokens[optind++]);
//            supervisor_t* supervisor = supervisor_get(instance);
//            if (supervisor == NULL) {
//                strcpy(response_str, "Invalid supervisor instance");
//                return;
//            }
//
//            supervisor_(supervisor, pid);
//            strcpy(response_str, "Service opened");
        } else if (strcmp(command, "close-service") == 0) {
            if (optind + 2 > number_of_tokens) {
                strcpy(response_str, "Not enough arguments for close-service");
                return;
            }
            char *service_name = command_tokens[optind++];
            int instance = atoi(command_tokens[optind++]);
            supervisor_t* supervisor = supervisor_get(instance);
            if (supervisor == NULL) {
                strcpy(response_str, "Invalid supervisor instance");
                return;
            }

            supervisor_close_service_wrapper(supervisor, service_name);
            strcpy(response_str, "Service closed");
        }

        else {
            strcpy(response_str, "Unknown command");
            syslog(LOG_ERR, "Unknown command: %s", command);
            return;
        }
    }

    for (int i = 0; i < 64; i++) {
        free(command_tokens[i]);
    }
}

void process_commands(int client_socket) {
    char buffer[256], response[256];
    int len = read(client_socket, buffer, sizeof(buffer) - 1);
    if (len == 0) {
        return;
    }
    buffer[len] = '\0';

    parse_command_arguments(buffer, response);

    printf("Received %s\n", buffer);
    write(client_socket, "ACK\n", 4);
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

    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);

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

int main() {
    openlog("supervisor", LOG_PID, LOG_DAEMON);
    syslog(LOG_NOTICE, "Supervisor daemon starting");
//    daemonize();

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
        process_commands(client_socket);
        close(client_socket);
    }

    close(server_socket);
    unlink(SOCKET_PATH);
    syslog(LOG_NOTICE, "Supervisor daemon terminated");

    closelog();

    return 0;
}