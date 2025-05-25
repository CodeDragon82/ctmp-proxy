#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>

#define SOURCE_PORT 33333
#define DESTINATION_PORT 44444
#define BUFFER_SIZE 70000
#define MAX_DESTINATION_CLIENTS 100

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t padding1;
    uint16_t length;
    uint32_t padding2;
    uint8_t data[];
} ctmp_packet;

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

int validate_packet(unsigned char *raw_packet, int packet_size) {

    if (packet_size < sizeof(ctmp_packet)) {
        return 0;
    }

    ctmp_packet *packet = (ctmp_packet *)raw_packet;

    if (packet->magic != 0xcc) {
        printf("Wrong magic! %u\n", packet->magic);
        return 0;
    }

    int expected_length = ntohs(packet->length);
    int actual_length = packet_size - sizeof(ctmp_packet);
    if (expected_length != actual_length) {
        printf("Wrong length!\n");
        return 0;
    }

    printf("Validate packet!\n");

    return 1;
}

int main(int argc, char const *argv[])
{
    fd_set socket_set;

    int source_socket = create_listener(SOURCE_PORT, 10);
    int destination_socket = create_listener(DESTINATION_PORT, 10);

    int source_client = -1;
    int destination_clients[MAX_DESTINATION_CLIENTS] = {0};

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
        if (FD_ISSET(source_socket, &socket_set)) {
            if (source_client == -1) printf("Source connection!\n");
            else printf("Source changed!\n");

            source_client = accept(source_socket, (struct sockaddr *)NULL, NULL);
        }

        // Check for connections on the destination socket.
        if (FD_ISSET(destination_socket, &socket_set)) {
            int new_destination_client = accept(destination_socket, (struct sockaddr *)NULL, NULL);
            for (int i = 0; i < MAX_DESTINATION_CLIENTS; i++) {
                if (destination_clients[i] == 0) {
                    destination_clients[i] = new_destination_client;
                    printf("New destination connection!\n");
                    break;
                }
            }
        }

        // Check for data from the source client.
        // Broadcast incoming data to the destination clients.
        if (source_client != -1 && FD_ISSET(source_client, &socket_set)) {
            int byte_count = recv(source_client, buffer, BUFFER_SIZE, 0);
            if (byte_count > 0) {

                if (validate_packet(buffer, byte_count)) {
                    for (int i = 0; i < MAX_DESTINATION_CLIENTS; i++) {
                    if (destination_clients[i] > 0) {
                        send(destination_clients[i], buffer, byte_count, 0);
                        }
                    }
                }
            } else if (byte_count == -1) {
                printf("Data error!\n");
            }
        }
    }


    return 0;
}
