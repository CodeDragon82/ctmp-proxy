#include "packet.h"
#include <netinet/in.h>
#include <stdio.h>

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

int validate_packet(ctmp_packet *packet, int packet_size) {

    if (packet_size < sizeof(ctmp_packet)) {
        return 0;
    }

    if (packet->magic != CTMP_MAGIC) {
        printf("Wrong magic! %u\n", packet->magic);
        return 0;
    }

    int expected_length = ntohs(packet->length);
    int actual_length = packet_size - sizeof(ctmp_packet);
    if (expected_length != actual_length) {
        printf("Wrong length!\n");
        return 0;
    }

    if (packet->options & CTMP_SENSITIVE_OPTION) {
        return check_checksum(packet, packet_size);
    }

    return 1;
}