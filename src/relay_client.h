#ifndef RELAY_CLIENT_H
#define RELAY_CLIENT_H

#include "packet.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int socket_fd;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE + 1U];
    uint64_t last_now_ms;
    int clock_failed;
} relay_client_t;

typedef enum {
    RELAY_IO_ERROR = -1,
    RELAY_IO_TIMEOUT = 0,
    RELAY_IO_PACKET = 1,
    RELAY_IO_INVALID = 2
} relay_io_result_t;

/* Resolve the relay and open a connected UDP socket. */
int relay_client_open(relay_client_t *client, const char *relay, uint16_t port);
/* Close the socket if it is open and mark the client as disconnected. */
void relay_client_close(relay_client_t *client);
/* Register this endpoint as a receiver for the given session. */
int relay_client_register_receiver(relay_client_t *client, const char *session);
/* Register this endpoint as a sender with relay impairment probabilities. */
int relay_client_register_sender(relay_client_t *client,
                                 const char *session,
                                 double loss,
                                 double corrupt,
                                 double duplicate);
/* Send one protocol datagram over the connected relay socket. */
int relay_client_send_datagram(relay_client_t *client,
                               const unsigned char *datagram,
                               size_t datagram_length);
/* Wait for a datagram and return packet, invalid-data, timeout, or I/O status. */
relay_io_result_t relay_client_receive_packet(relay_client_t *client,
                                              uint64_t timeout_ms,
                                              packet_t *packet);
/* Read monotonic time in milliseconds and retain the last value on failure. */
uint64_t relay_client_now(relay_client_t *client);
/* Check whether the client's last clock read succeeded. */
int relay_client_clock_ok(const relay_client_t *client);

#endif