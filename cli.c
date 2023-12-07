#include <sys/socket.h>
#include <sys/un.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define SOCKET_PATH "/tmp/supervisor_daemon.sock"

int main(int argc, char *argv[]) {
    struct sockaddr_un addr;
    int socket_fd;
    char buffer[256];

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
        perror("connect");
        exit(EXIT_FAILURE);
    }

    write(socket_fd, argv[1], strlen(argv[1]));
    read(socket_fd, buffer, sizeof(buffer));
    printf("Response from daemon: %s\n", buffer);

    close(socket_fd);
    return 0;
}
