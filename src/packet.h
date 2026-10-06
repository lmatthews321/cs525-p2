#ifndef PACKET_H
#define PACKET_H

#include <stddef.h>
#include <stdint.h>

#define PACKET_HEADER_SIZE 10U
#define PACKET_MAX_PAYLOAD 1024U
#define PACKET_MAX_DATAGRAM_SIZE (PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD)

typedef enum {
    PACKET_DATA = 0,
    PACKET_ACK = 1,
    PACKET_FIN = 2
} packet_type_t;

typedef struct {
    packet_type_t type;
    uint32_t seq;
    size_t payload_length;
    unsigned char payload[PACKET_MAX_PAYLOAD];
} packet_t;

/* Return the Internet checksum for the supplied bytes. */
uint16_t packet_checksum(const unsigned char *bytes, size_t length);
/* Encode a packet when the output buffer has enough capacity. */
int packet_encode(const packet_t *packet,
                  unsigned char *datagram,
                  size_t capacity,
                  size_t *datagram_length);
/* Decode only datagrams with a valid header, length, type, and checksum. */
int packet_decode(const unsigned char *datagram,
                  size_t datagram_length,
                  packet_t *packet);

#endif