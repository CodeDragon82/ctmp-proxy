#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

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
        perror("Error while creating socket!");
        exit(EXIT_FAILURE);
    }

    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = htonl(INADDR_ANY);

    if (bind(socket_fd, (const struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed!");
        exit(EXIT_FAILURE);
    }

    if (listen(socket_fd, max_clients) < 0) {
        perror("Lisen failed!");
        exit(EXIT_FAILURE);
    }

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

void broadcast(int *destination_clients, char *packet, int packet_size) {
    for (int i = 0; i < MAX_DESTINATION_CLIENTS; i++) {
        if (destination_clients[i] > 0) {
            int bytes_sent = send(destination_clients[i], packet, packet_size, 0);
            if (bytes_sent < 0) {
                perror("Failed to send data to destination client");
            } else {
                printf("Send %d bytes to %d\n", bytes_sent, destination_clients[i]);
            }
        }
    }
}

/*
 * Check each destination client connection and close any that are inactive.
 */
void check_destination_disconnects(int *destination_clients, fd_set *socket_set) {
    for (int i = 0; i < MAX_DESTINATION_CLIENTS; i++) {
        if (destination_clients[i] > 0 && FD_ISSET(destination_clients[i], socket_set)) {
            char tmp_buf[1];
            int result = recv(destination_clients[i], tmp_buf, 1, MSG_PEEK | MSG_DONTWAIT);
            if (result <= 0) {
                printf("Destination client %d disconnected and removed\n", i);
                close(destination_clients[i]);
                destination_clients[i] = 0;
            } 
        }
    }
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
            perror("Error selecting FD!");
            continue;
        }

        // Check for connections on the source socket.
        if (FD_ISSET(source_socket, &socket_set)) {
            if (source_client != -1) {
                printf("Closing old source connection.\n");
                close(source_client);
            }

            source_client = accept(source_socket, (struct sockaddr *)NULL, NULL);
            if (source_client < 0) {
                perror("New source connection failed");
            } else {
                printf("New source connection: %d\n", source_client);
            }
        }

        // Check for connections on the destination socket.
        if (FD_ISSET(destination_socket, &socket_set)) {
            int new_destination_client = accept(destination_socket, (struct sockaddr *)NULL, NULL);
            if (new_destination_client < 0) {
                perror("New destination connection failed");
            } else {
                for (int i = 0; i < MAX_DESTINATION_CLIENTS; i++) {
                    if (destination_clients[i] == 0) {
                        destination_clients[i] = new_destination_client;
                        printf("New destination connection: %d\n", new_destination_client);
                        break;
                    }
                }
            }
        }

        // Check for data from the source client.
        // Broadcast incoming data to the destination clients.
        if (source_client != -1 && FD_ISSET(source_client, &socket_set)) {
            int byte_count = recv(source_client, buffer, BUFFER_SIZE, 0);
            if (byte_count > 0) {
                if (validate_packet(buffer, byte_count)) {
                    broadcast(destination_clients, buffer, byte_count);
                }
            } else {
                printf("Source client disconnected\n");
                close(source_client);
                source_client = -1;
            }
        }

        check_destination_disconnects(destination_clients, &socket_set);
    }


    return 0;
}
