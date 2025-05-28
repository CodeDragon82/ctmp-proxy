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
    uint8_t options;
    uint16_t length;
    uint16_t checksum;
    uint16_t padding;
    uint8_t data[];
} ctmp_packet;

/*
 * Setup a listener socket on a given port.
 */
int create_listener(int port) {
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

    if (listen(socket_fd, 100) < 0) {
        perror("Listen failed!");
        exit(EXIT_FAILURE);
    }

    return socket_fd;
}

/*
 * Calculates the packet's checksum based on the 'Internet Checksum' standard
 * defined in RFC 1071.
 */
int calculate_checksum(unsigned char *packet_data, int packet_size) {
    uint32_t sum = 0;

    for (int i = 0; i < packet_size - 1; i += 2) {
        uint16_t word = packet_data[i] << 8;

        if (i + 1 < packet_size) {
            word |= packet_data[i + 1];
        }

        sum += word;

        // Fold the carry bits.
        if (sum > 0xFFFF) {
            sum = (sum & 0xFFFF) + 1;
        }
    }

    return (uint16_t)~sum;
}

/*
 * Calculates the checksum of the packet and compares it to the expected
 * checksum defined within the packet.
 */
int check_checksum(ctmp_packet *packet, int packet_size) {
    int expected_checksum = packet->checksum;

    packet->checksum = 0xCCCC;
    int actual_checksum = calculate_checksum((unsigned char *)packet, packet_size);
    packet->checksum = expected_checksum;

    if (ntohs(expected_checksum) != actual_checksum) {
        printf("Wrong checksum! Expected %d but got %d\n", expected_checksum, actual_checksum);
        return 0;
    }

    return 1;
}

/*
 * Casts the raw data received from the source client into a CTMP packet.
 * Then valids the fields of the CTMP packet.
 */
int validate_packet(unsigned char *raw_data, int data_size) {

    if (data_size < sizeof(ctmp_packet)) {
        return 0;
    }

    ctmp_packet *packet = (ctmp_packet *)raw_data;

    if (packet->magic != 0xcc) {
        printf("Wrong magic! %u\n", packet->magic);
        return 0;
    }

    int expected_length = ntohs(packet->length);
    int actual_length = data_size - sizeof(ctmp_packet);
    if (expected_length != actual_length) {
        printf("Wrong length!\n");
        return 0;
    }

    if (packet->options & 0x40) {
        return check_checksum(packet, data_size);
    }

    return 1;
}

/*
 * Send packet data to all destination clients.
 */
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

/*
 * Handle a new client connection on the destination socket.
 */
void connect_destination(int *destination_clients, int destination_socket) {
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

/*
 * Handle a new client connection on the source socket.
 */
int connect_source(int previous_source, int source_socket) {

    // If an old source client is connection, disconnect it and allow for the new source client.
    if (previous_source != -1) {
        printf("Closing old source connection.\n");
        close(previous_source);
        return -1;
    }

    int new_source = accept(source_socket, (struct sockaddr *)NULL, NULL);

    if (new_source < 0) perror("New source connection failed");
    else printf("New source connection: %d\n", new_source);

    return new_source;
}

/*
 * Reads data received from the source client, validates the packet, and
 * broadcasts it to the destination clients.
 * 
 * Returns the number of bytes received from the source client.
 */
int relay(int source_client, int *destination_clients) {
    char buffer[BUFFER_SIZE];

    int byte_count = recv(source_client, buffer, BUFFER_SIZE, 0);
    if (byte_count > 0) {
        printf("Received %d bytes from %d\n", byte_count, source_client);
        if (validate_packet(buffer, byte_count)) {
            printf("Broadcasting valid packet\n");
            broadcast(destination_clients, buffer, byte_count);
        } else {
            printf("Invalid packet\n");
        }
    }

    return byte_count;
}

int main(int argc, char const *argv[])
{
    fd_set socket_set;

    int source_socket = create_listener(SOURCE_PORT);
    int destination_socket = create_listener(DESTINATION_PORT);

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
        for (int i = 0; i < MAX_DESTINATION_CLIENTS; i++) {
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
            source_client = connect_source(source_client, source_socket);
        }

        // Check for new connections on the destination socket.
        if (FD_ISSET(destination_socket, &socket_set)) {
            connect_destination(destination_clients, destination_socket);
        }

        // Check for data from the source client.
        // Broadcast incoming data to the destination clients.
        if (FD_ISSET(source_client, &socket_set)) {
            if (relay(source_client, destination_clients) < 0) {
                printf("Source client disconnected\n");
                close(source_client);
                source_client = -1;
            }
        }

        check_destination_disconnects(destination_clients, &socket_set);
    }

    return 0;
}
