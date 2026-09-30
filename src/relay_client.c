#define _POSIX_C_SOURCE 200809L

#include "relay_client.h"
#include "receiver.h"

#include <errno.h>
#include <math.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

static int monotonic_now_ms(uint64_t *now_ms)
{
    struct timespec current_time;

    if (clock_gettime(CLOCK_MONOTONIC, &current_time) != 0) {
        return 0;
    }
    *now_ms = (uint64_t)current_time.tv_sec * UINT64_C(1000) +
              (uint64_t)current_time.tv_nsec / UINT64_C(1000000);
    return 1;
}

static int wait_for_readable(int socket_fd, uint64_t timeout_ms)
{
    uint64_t now_ms;
    uint64_t deadline_ms;

    if (!monotonic_now_ms(&now_ms)) {
        return -1;
    }
    deadline_ms = now_ms > UINT64_MAX - timeout_ms ? UINT64_MAX : now_ms + timeout_ms;

    for (;;) {
        struct pollfd descriptor = {socket_fd, POLLIN, 0};
        uint64_t remaining_ms;
        int poll_timeout;
        int poll_status;

        if (!monotonic_now_ms(&now_ms)) {
            return -1;
        }
        if (now_ms >= deadline_ms) {
            return 0;
        }
        remaining_ms = deadline_ms - now_ms;
        poll_timeout = remaining_ms > (uint64_t)INT_MAX ? INT_MAX : (int)remaining_ms;
        poll_status = poll(&descriptor, 1, poll_timeout);
        if (poll_status < 0) {
            if (errno == EINTR) {
                continue;
            }
            return -1;
        }
        if (poll_status == 0) {
            return 0;
        }
        if ((descriptor.revents & POLLIN) != 0) {
            return 1;
        }
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) {
            return -1;
        }
    }
}

int relay_client_open(relay_client_t *client, const char *relay, uint16_t port)
{
    struct addrinfo hints = {0};
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    char service[6];
    int address_status;
    int last_error = 0;

    if (client == NULL || relay == NULL) {
        return 0;
    }
    client->socket_fd = -1;
    client->last_now_ms = 0;
    client->clock_failed = 0;
    (void)snprintf(service, sizeof(service), "%u", (unsigned int)port);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    address_status = getaddrinfo(relay, service, &hints, &addresses);
    if (address_status != 0) {
        fprintf(stderr, "Cannot resolve relay '%s': %s\n", relay, gai_strerror(address_status));
        return 0;
    }

    for (address = addresses; address != NULL; address = address->ai_next) {
        int candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        if (candidate < 0) {
            last_error = errno;
            continue;
        }
        if (connect(candidate, address->ai_addr, address->ai_addrlen) == 0) {
            client->socket_fd = candidate;
            break;
        }
        last_error = errno;
        (void)close(candidate);
    }
    freeaddrinfo(addresses);
    if (client->socket_fd < 0) {
        fprintf(stderr, "Cannot connect UDP socket to relay '%s': %s\n",
                relay, strerror(last_error));
        return 0;
    }
    return 1;
}

void relay_client_close(relay_client_t *client)
{
    if (client != NULL && client->socket_fd >= 0) {
        (void)close(client->socket_fd);
        client->socket_fd = -1;
    }
}

static int register_hello(relay_client_t *client, const char *hello, size_t hello_length)
{
    int attempt;

    for (attempt = 0; attempt < 5; attempt++) {
        ssize_t sent;

        do {
            sent = send(client->socket_fd, hello, hello_length, 0);
        } while (sent < 0 && errno == EINTR);
        if (sent < 0 || (size_t)sent != hello_length) {
            fprintf(stderr, "Could not send relay registration: %s\n", strerror(errno));
            return 0;
        }

        for (;;) {
            char reply[256];
            ssize_t reply_length;
            int ready = wait_for_readable(client->socket_fd, UINT64_C(1000));

            if (ready < 0) {
                fprintf(stderr, "Network error waiting for relay registration: %s\n",
                        strerror(errno));
                return 0;
            }
            if (ready == 0) {
                break;
            }
            reply_length = recv(client->socket_fd, reply, sizeof(reply), 0);
            if (reply_length < 0) {
                if (errno == EINTR) {
                    continue;
                }
                fprintf(stderr, "Could not receive relay registration reply: %s\n",
                        strerror(errno));
                return 0;
            }
            if (reply_length == 2 && memcmp(reply, "OK", 2) == 0) {
                return 1;
            }
            if (reply_length >= 4 && memcmp(reply, "ERR ", 4) == 0) {
                fprintf(stderr, "Relay refused registration: %.*s\n",
                        (int)(reply_length - 4), reply + 4);
                return 0;
            }
            fprintf(stderr, "Relay returned an invalid registration reply.\n");
            return 0;
        }
    }

    fprintf(stderr, "Relay did not answer registration after five attempts.\n");
    return 0;
}

int relay_client_register_receiver(relay_client_t *client, const char *session)
{
    char hello[48];
    int hello_length;

    if (client == NULL || client->socket_fd < 0 || !receiver_session_valid(session)) {
        return 0;
    }
    hello_length = snprintf(hello, sizeof(hello), "HELLO %s recv", session);
    if (hello_length < 0 || (size_t)hello_length >= sizeof(hello)) {
        return 0;
    }
    return register_hello(client, hello, (size_t)hello_length);
}

int relay_client_register_sender(relay_client_t *client,
                                 const char *session,
                                 double loss,
                                 double corrupt,
                                 double duplicate)
{
    char hello[128];
    int hello_length;

    if (client == NULL || client->socket_fd < 0 || !receiver_session_valid(session) ||
        !isfinite(loss) || !isfinite(corrupt) || !isfinite(duplicate) ||
        loss < 0.0 || loss > 0.5 || corrupt < 0.0 || corrupt > 0.5 ||
        duplicate < 0.0 || duplicate > 0.5) {
        return 0;
    }
    hello_length = snprintf(hello, sizeof(hello), "HELLO %s send %.15g %.15g %.15g",
                            session, loss, corrupt, duplicate);
    if (hello_length < 0 || (size_t)hello_length >= sizeof(hello)) {
        return 0;
    }
    return register_hello(client, hello, (size_t)hello_length);
}

int relay_client_send_datagram(relay_client_t *client,
                               const unsigned char *datagram,
                               size_t datagram_length)
{
    ssize_t sent;

    if (client == NULL || client->socket_fd < 0 || datagram == NULL) {
        return -1;
    }
    do {
        sent = send(client->socket_fd, datagram, datagram_length, 0);
    } while (sent < 0 && errno == EINTR);
    return sent >= 0 && (size_t)sent == datagram_length ? 0 : -1;
}

relay_io_result_t relay_client_receive_packet(relay_client_t *client,
                                              uint64_t timeout_ms,
                                              packet_t *packet)
{
    int ready;
    ssize_t received;

    if (client == NULL || client->socket_fd < 0 || client->clock_failed) {
        return RELAY_IO_ERROR;
    }
    ready = wait_for_readable(client->socket_fd, timeout_ms);
    if (ready < 0) {
        return RELAY_IO_ERROR;
    }
    if (ready == 0) {
        return RELAY_IO_TIMEOUT;
    }
    received = recv(client->socket_fd, client->datagram, sizeof(client->datagram), 0);
    if (received < 0) {
        return errno == EINTR ? RELAY_IO_TIMEOUT : RELAY_IO_ERROR;
    }
    if (!packet_decode(client->datagram, (size_t)received, packet)) {
        return RELAY_IO_INVALID;
    }
    return RELAY_IO_PACKET;
}

uint64_t relay_client_now(relay_client_t *client)
{
    uint64_t now_ms;

    if (client == NULL || !monotonic_now_ms(&now_ms)) {
        if (client != NULL) {
            client->clock_failed = 1;
        }
        return client == NULL ? 0 : client->last_now_ms;
    }
    client->last_now_ms = now_ms;
    return now_ms;
}

int relay_client_clock_ok(const relay_client_t *client)
{
    return client != NULL && !client->clock_failed;
}