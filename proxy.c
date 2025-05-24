#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SOURCE_PORT 33333
#define DESTINATION_PORT 44444
#define BUFFER_SIZE 1024

int create_listener(int port, int max_clients) {
    int socket_fd;
    struct sockaddr_in address;

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        printf("Error while creating socket!\n");
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd, (const struct sockaddr *)&address, sizeof(address)) < 0) {
        printf("Bind failed!\n");
    }

    listen(socket_fd, max_clients);

    return socket_fd;
}

int main(int argc, char const *argv[])
{
    fd_set socket_set;

    int source_socket = create_listener(SOURCE_PORT, 10);
    int destination_socket = create_listener(DESTINATION_PORT, 10);

    int source_client = -1;
    int destination_clients[100] = {0};

    char buffer[BUFFER_SIZE];

    while(1) {

        // Define a FD set to monitor connections.
        FD_ZERO(&socket_set);
        FD_SET(source_socket, &socket_set);
        FD_SET(destination_socket, &socket_set);
        int max_fd = source_socket > destination_socket ? source_socket : destination_socket;
        if (source_client != -1) {
            FD_SET(source_client, &socket_set);
            if (source_client > max_fd) max_fd = source_client;
        }
        for (int i = 0; i < 100; i++) {
            int destination_client = destination_clients[i];
            if (destination_client > 0) {
                FD_SET(destination_client, &socket_set);
                if (destination_client > max_fd) max_fd = destination_client;
            }
        }

        if (select(max_fd + 1, &socket_set, NULL, NULL, NULL) < 0) {
            printf("Error selecting FD!\n");
        }

        // Check for connections on the source socket.
        if (FD_ISSET(source_socket, &socket_set) && source_client == -1) {
            source_client = accept(source_socket, (struct sockaddr *)NULL, NULL);
            printf("Source connection!\n");
        }

        // Check for connections on the destination socket.
        if (FD_ISSET(destination_socket, &socket_set)) {
            int new_destination_client = accept(destination_socket, (struct sockaddr *)NULL, NULL);
            for (int i = 0; i < 100; i++) {
                if (destination_clients[i] == 0) {
                    destination_clients[i] = new_destination_client;
                    printf("New destination connection!\n");
                    break;
                }
            }
        }
    }


    return 0;
}
