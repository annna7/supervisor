#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>

#define SOCKET_PATH "/tmp/supervisor_daemon.sock"

int main(int argc, char *argv[]) {
    struct sockaddr_un addr;
    int socket_fd;

    if (argc < 2) {
        printf("Usage: %s <command>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    socket_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket");
        exit(EXIT_FAILURE);
    }

    memset(&addr, 0, sizeof(struct sockaddr_un));
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (connect(socket_fd, (struct sockaddr *) &addr, sizeof(struct sockaddr_un)) < 0) {
        perror("supervisor daemon connection");
        exit(EXIT_FAILURE);
    }

    char buffer[1024] = {0}; // concatenate arguments

    for (int i = 1; i < argc; i++) {
        strcat(buffer, argv[i]);
        strcat(buffer, " ");
    }

    // write response to socket
    write(socket_fd, buffer, strlen(buffer));

    memset(buffer, 0, sizeof(buffer));
    // read response from daemon via the socket
    read(socket_fd, buffer, sizeof(buffer));

    printf("%s\n", buffer);

    close(socket_fd);
    return 0;
}
