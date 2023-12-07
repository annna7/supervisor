#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/stat.h>
#include <syslog.h>
#include <getopt.h>
#include "utils.h"

#define SOCKET_PATH "/tmp/supervisor_daemon.sock"

void parse_command_arguments(char *command_str, char *response_str) {
    int number_of_tokens;
    char *command_tokens[64];
    for (int i = 0; i < 64; i++) {
        command_tokens[i] = malloc(64 * sizeof(char));
    }

    parse_string(command_str, &number_of_tokens, command_tokens);

    printf("Number of tokens: %d\n", number_of_tokens);

    char *command = NULL;
    if (number_of_tokens > 0) {
        command = command_tokens[0];
    }

    if (command == NULL) {
        strcpy(response_str, "No command provided");
        printf("No command provided\n");
        // Perform necessary cleanup before exiting
//        exit(EXIT_FAILURE);
    }

    if (strcmp(command, "init") == 0) {
        strcpy(response_str, "Init");
        printf("Init\n");
        // TODO: supervisor_init(instance);
    } else if (strcmp(command, "close") == 0) {
        strcpy(response_str, "Close");
        printf("Close\n");
        // TODO: supervisor_close(instance);
    } else if (strcmp(command, "create-service") == 0) {
        if (number_of_tokens < 5) {
            printf("Not enough arguments for create-service\n");
            exit(EXIT_FAILURE);
        }

        char *service_name = command_tokens[1];
        char *program_path = command_tokens[2];
        char *argv_service = command_tokens[3];
        int argc_service = atoi(command_tokens[4]);

        // TODO: supervisor_create_service(instance, service_name, program_path, argv_service, argc_service, create_stopped, restart_times);
    } else {
        strcpy(response_str, "Unknown command");
        syslog(LOG_ERR, "Unknown command: %s", command);
    }

    printf("REACHED END\n");
}

//void parse_command_arguments(char *command_str, char *response_str) {
//    int number_of_tokens;
//    char *command_tokens[64];
//    for (int i = 0; i < 64; i++) {
//        command_tokens[i] = malloc(64 * sizeof(char));
//    }
//
//    parse_string(command_str, &number_of_tokens, command_tokens);
//
//    printf("Number of tokens: %d\n", number_of_tokens);
//
//    int instance = 0;
//    int create_stopped = 0;
//    int restart_times = 0;
//
//    struct option long_options[] = {
//            {"instance", required_argument, 0, 'i'},
//            {"create-stopped", no_argument, &create_stopped, 1},
//            {"restart-times", required_argument, 0, 'r'},
//            {0, 0, 0, 0}
//    };
//
//    int option_index = 0;
//    int c;
//
//    optind = 1;
//
//    printf("Number of tokens: %d\n", number_of_tokens);
//
//    while ((c = getopt_long(number_of_tokens, command_tokens, "i:r:", long_options, &option_index)) != -1) {
//        switch (c) {
//            case 'i':
//                instance = atoi(optarg);
//                break;
//            case 'r':
//                restart_times = atoi(optarg);
//                break;
//            case '?':
//                break;
//            default:
//                printf("?? getopt returned character code 0%o ??\n", c);
//                break;
//        }
//    }
//
//    char *command = NULL;
//    if (optind < number_of_tokens) {
//        command = command_tokens[optind++];
//    }
//
//    if (command == NULL) {
//        response_str = "No command provided\n";
//        printf("No command provided\n");
//        // Perform necessary cleanup before exiting
//        exit(EXIT_FAILURE);
//    }
//
//    if (strcmp(command, "init") == 0) {
//        response_str = "Init\n";
//        printf("Init\n");
//        // TODO: supervisor_init(instance);
//    } else if (strcmp(command, "close") == 0) {
//        response_str = "Close\n";
//        printf("Close\n");
//        // TODO: supervisor_close(instance);
//    } else if (strcmp(command, "create-service") == 0) {
//        if (optind + 3 > number_of_tokens) {
//            printf("Not enough arguments for create-service\n");
//            exit(EXIT_FAILURE);
//        }
//
//        char *service_name = command_tokens[optind++];
//        char *program_path = command_tokens[optind++];
//        char *argv_service = command_tokens[optind++];
//        int argc_service = atoi(command_tokens[optind++]);
//
//        // TODO: supervisor_create_service(instance, service_name, program_path, argv_service, argc_service, create_stopped, restart_times);
//    } else {
//        syslog(LOG_ERR, "Unknown command: %s", command);
//        exit(EXIT_FAILURE);
//    }
//
//    for (int i = 0; i < 64; i++) {
//        free(command_tokens[i]);
//    }
////    strcpy(response_str, "ACK");
//}

void process_commands(int client_socket) {
    char buffer[256], response[256];
    int len = read(client_socket, buffer, sizeof(buffer) - 1);
    if (len == 0) {
        return;
    }
    buffer[len] = '\0';

    printf("Received command: %s\n", buffer);

    parse_command_arguments(buffer, response);

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

    openlog("supervisor_daemon", LOG_PID, LOG_DAEMON);
}

int main() {
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

    while (1) {
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