#include "packet.h"

#include <arpa/inet.h>
#include <string.h>

enum {
    PACKET_TYPE_OFFSET = 0,
    PACKET_RESERVED_OFFSET = 1,
    PACKET_CHECKSUM_OFFSET = 2,
    PACKET_SEQUENCE_OFFSET = 4,
    PACKET_LENGTH_OFFSET = 8,
    PACKET_PAYLOAD_OFFSET = 10
};

/* Compute the Internet checksum over a byte buffer, including an odd final byte. */
uint16_t packet_checksum(const unsigned char *bytes, size_t length)
{
    uint32_t sum = 0;
    size_t index = 0;

    if (bytes == NULL && length != 0) {
        return 0;
    }
    while (index + 1 < length) {
        sum += ((uint32_t)bytes[index] << 8) | bytes[index + 1];
        index += 2;
    }
    if (index < length) {
        sum += (uint32_t)bytes[index] << 8;
    }
    while ((sum >> 16) != 0) {
        sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

/* Reject packet values that cannot be represented by the wire protocol. */
static int packet_valid(const packet_t *packet)
{
    if (packet == NULL || packet->payload_length > PACKET_MAX_PAYLOAD ||
        (packet->type != PACKET_DATA && packet->type != PACKET_ACK &&
         packet->type != PACKET_FIN)) {
        return 0;
    }
    return packet->type == PACKET_DATA || packet->payload_length == 0;
}

/* Serialize a validated packet and add its checksum in network byte order. */
int packet_encode(const packet_t *packet,
                  unsigned char *datagram,
                  size_t capacity,
                  size_t *datagram_length)
{
    uint16_t network_length;
    uint16_t network_checksum;
    uint32_t network_sequence;
    size_t packet_length;

    if (!packet_valid(packet) || datagram == NULL || datagram_length == NULL) {
        return 0;
    }
    packet_length = PACKET_HEADER_SIZE + packet->payload_length;
    if (capacity < packet_length) {
        return 0;
    }

    datagram[PACKET_TYPE_OFFSET] = (unsigned char)packet->type;
    datagram[PACKET_RESERVED_OFFSET] = 0;
    datagram[PACKET_CHECKSUM_OFFSET] = 0;
    datagram[PACKET_CHECKSUM_OFFSET + 1] = 0;
    network_sequence = htonl(packet->seq);
    memcpy(datagram + PACKET_SEQUENCE_OFFSET, &network_sequence, sizeof(network_sequence));
    network_length = htons((uint16_t)packet->payload_length);
    memcpy(datagram + PACKET_LENGTH_OFFSET, &network_length, sizeof(network_length));
    if (packet->payload_length > 0) {
        memcpy(datagram + PACKET_PAYLOAD_OFFSET, packet->payload, packet->payload_length);
    }

    network_checksum = htons(packet_checksum(datagram, packet_length));
    memcpy(datagram + PACKET_CHECKSUM_OFFSET, &network_checksum, sizeof(network_checksum));
    *datagram_length = packet_length;
    return 1;
}

/* Validate a wire datagram before decoding its header and payload into a packet. */
int packet_decode(const unsigned char *datagram,
                  size_t datagram_length,
                  packet_t *packet)
{
    uint16_t network_length;
    uint32_t network_sequence;
    size_t payload_length;

    if (datagram == NULL || packet == NULL || datagram_length < PACKET_HEADER_SIZE ||
        datagram[PACKET_RESERVED_OFFSET] != 0 ||
        datagram[PACKET_TYPE_OFFSET] > PACKET_FIN) {
        return 0;
    }
    memcpy(&network_length, datagram + PACKET_LENGTH_OFFSET, sizeof(network_length));
    payload_length = ntohs(network_length);
    if (payload_length > PACKET_MAX_PAYLOAD ||
        PACKET_HEADER_SIZE + payload_length != datagram_length ||
        (datagram[PACKET_TYPE_OFFSET] != PACKET_DATA && payload_length != 0) ||
        packet_checksum(datagram, datagram_length) != 0) {
        return 0;
    }

    memcpy(&network_sequence, datagram + PACKET_SEQUENCE_OFFSET, sizeof(network_sequence));
    packet->type = (packet_type_t)datagram[PACKET_TYPE_OFFSET];
    packet->seq = ntohl(network_sequence);
    packet->payload_length = payload_length;
    if (payload_length > 0) {
        memcpy(packet->payload, datagram + PACKET_PAYLOAD_OFFSET, payload_length);
    }
    return 1;
}