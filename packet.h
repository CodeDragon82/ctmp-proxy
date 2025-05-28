#include <stdint.h>

#define CTMP_MAGIC        0xCC
#define CTMP_SENSITIVE_OPTION 0x40

typedef struct __attribute__((packed)) {
    uint8_t magic;
    uint8_t options;
    uint16_t length;
    uint16_t checksum;
    uint16_t padding;
    uint8_t data[];
} ctmp_packet;

/*
 * Calculates the packet's checksum based on the 'Internet Checksum' standard
 * defined in RFC 1071.
 */
int calculate_checksum(unsigned char *data, int size);

/*
 * Calculates the checksum of the packet and compares it to the expected
 * checksum defined within the packet.
 */
int check_checksum(ctmp_packet *packet, int packet_size);

/*
 * Performs the following checks on the packet:
 * - The packet must be atleast the size of the header (i.e., 16 bytes).
 * - The magic field must be 0xCC.
 * - The size of the packet must match the length field.
 * - If the sensitive bit is set in the option field, validate the checksum.
 */
int validate_packet(ctmp_packet *packet, int packet_size);