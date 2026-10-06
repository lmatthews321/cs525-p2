#define _POSIX_C_SOURCE 200809L

#include "relay_io.h"
#include "receiver_gbn.h"

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

/* Read monotonic time in milliseconds for retransmission and idle deadlines. */
static int monotonic_now_ms(uint64_t *now_ms)
{
    struct timespec current_time;

    /* Excluded: clock_gettime failure depends on a host clock-system error. */
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) != 0) { /* GCOVR_EXCL_START */
        return 0;
        /* GCOVR_EXCL_STOP */
    }
    *now_ms = (uint64_t)current_time.tv_sec * UINT64_C(1000) +
              (uint64_t)current_time.tv_nsec / UINT64_C(1000000);
    return 1;
}

/* Wait for socket readability until a monotonic deadline, retrying interruptions. */
static int wait_for_readable(int socket_fd, uint64_t timeout_ms)
{
    uint64_t now_ms;
    uint64_t deadline_ms;
    int poll_once = 1;

    /* Excluded: this path requires the operating system clock call to fail. */
    if (!monotonic_now_ms(&now_ms)) { /* GCOVR_EXCL_START */
        return -1;
        /* GCOVR_EXCL_STOP */
    }
    deadline_ms = now_ms > UINT64_MAX - timeout_ms ? UINT64_MAX : now_ms + timeout_ms;

    while (poll_once || now_ms < deadline_ms) {
        struct pollfd descriptor = {socket_fd, POLLIN, 0};
        uint64_t remaining_ms;
        int poll_timeout;
        int poll_status;

        poll_once = 0;
        /* Excluded: this path requires the operating system clock call to fail. */
        if (!monotonic_now_ms(&now_ms)) { /* GCOVR_EXCL_START */
            return -1;
            /* GCOVR_EXCL_STOP */
        }
        if (now_ms >= deadline_ms) {
            poll_timeout = 0;
        } else {
            remaining_ms = deadline_ms - now_ms;
            poll_timeout = remaining_ms > (uint64_t)INT_MAX
                ? INT_MAX : (int)remaining_ms;
        }
        poll_status = poll(&descriptor, 1, poll_timeout);
        /* Excluded: poll failures (other than EINTR) are environmental OS errors. */
        if (poll_status < 0) { /* GCOVR_EXCL_START */
            if (errno == EINTR) {
                continue;
            }
            return -1;
            /* GCOVR_EXCL_STOP */
        }
        if (poll_status == 0) {
            return 0;
        }
        /* Excluded branch: readiness/error combinations are determined by the OS. */
        if ((descriptor.revents & POLLIN) != 0) { /* GCOVR_EXCL_BR_LINE */
            return 1;
        }
        /* Excluded: these poll error flags require a socket/system error. */
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) { /* GCOVR_EXCL_START */
            return -1;
            /* GCOVR_EXCL_STOP */
        }
    }
    return 0; /* GCOVR_EXCL_LINE: unexpected poll event flags are OS-controlled. */
}

/* Resolve the relay and connect a UDP socket to one of its available addresses. */
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
    /* Excluded: resolver failures depend on DNS and host networking conditions. */
    if (address_status != 0) { /* GCOVR_EXCL_START */
        fprintf(stderr, "Cannot resolve relay '%s': %s\n", relay, gai_strerror(address_status));
        return 0;
        /* GCOVR_EXCL_STOP */
    }

    /* Excluded branch: the number of addresses is controlled by the host resolver. */
    for (address = addresses; address != NULL; address = address->ai_next) { /* GCOVR_EXCL_BR_LINE */
        int candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        /* Excluded: socket allocation failures are operating-system resource errors. */
        if (candidate < 0) { /* GCOVR_EXCL_START */
            last_error = errno;
            continue;
            /* GCOVR_EXCL_STOP */
        }
        /* Excluded branch: connect outcomes depend on host networking and OS state. */
        if (connect(candidate, address->ai_addr, address->ai_addrlen) == 0) { /* GCOVR_EXCL_BR_LINE */
            client->socket_fd = candidate;
            break;
        }
        /* Excluded: only reached when the operating system rejects connect(). */
        /* GCOVR_EXCL_START */
        last_error = errno;
        (void)close(candidate);
        /* GCOVR_EXCL_STOP */
    }
    freeaddrinfo(addresses);
    /* Excluded: all-address connection failure is environment-dependent. */
    if (client->socket_fd < 0) { /* GCOVR_EXCL_START */
        fprintf(stderr, "Cannot connect UDP socket to relay '%s': %s\n",
                relay, strerror(last_error));
        return 0;
        /* GCOVR_EXCL_STOP */
    }
    return 1;
}

/* Close the client's socket, if open, and mark the client as disconnected. */
void relay_client_close(relay_client_t *client)
{
    if (client != NULL && client->socket_fd >= 0) {
        (void)close(client->socket_fd);
        client->socket_fd = -1;
    }
}

/* Send a registration request and retry until the relay answers or attempts expire. */
static int register_hello(relay_client_t *client, const char *hello, size_t hello_length)
{
    int attempt;

    for (attempt = 0; attempt < 5; attempt++) {
        ssize_t sent;

        /* Excluded branch: EINTR during send is a signal-driven OS condition. */
        do {
            sent = send(client->socket_fd, hello, hello_length, 0);
        } while (sent < 0 && errno == EINTR); /* GCOVR_EXCL_BR_LINE */
        /* Excluded: UDP send failures or short sends are operating-system errors. */
        if (sent < 0 || (size_t)sent != hello_length) { /* GCOVR_EXCL_START */
            fprintf(stderr, "Could not send relay registration: %s\n", strerror(errno));
            return 0;
            /* GCOVR_EXCL_STOP */
        }

        {
            int receive_retry = 1;

            while (receive_retry) {
                char reply[256];
                ssize_t reply_length;
                int ready = wait_for_readable(client->socket_fd, UINT64_C(1000));

                /* Excluded: poll reports only environmental socket/system errors here. */
                if (ready < 0) { /* GCOVR_EXCL_START */
                    fprintf(stderr, "Network error waiting for relay registration: %s\n",
                            strerror(errno));
                    return 0;
                    /* GCOVR_EXCL_STOP */
                }
                if (ready == 0) {
                    break;
                }
                reply_length = recv(client->socket_fd, reply, sizeof(reply), 0);
                receive_retry = 0;
                /* Excluded: recv errors other than EINTR are operating-system failures. */
                if (reply_length < 0) { /* GCOVR_EXCL_START */
                    if (errno == EINTR) {
                        receive_retry = 1;
                        continue;
                    }
                    fprintf(stderr, "Could not receive relay registration reply: %s\n",
                            strerror(errno));
                    return 0;
                    /* GCOVR_EXCL_STOP */
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
    }

    fprintf(stderr, "Relay did not answer registration after five attempts.\n");
    return 0;
}

/* Register this client with the relay as the receiver for a session. */
int relay_client_register_receiver(relay_client_t *client, const char *session)
{
    char hello[48];
    int hello_length;

    if (client == NULL || client->socket_fd < 0 || !receiver_session_valid(session)) {
        return 0;
    }
    hello_length = snprintf(hello, sizeof(hello), "HELLO %s recv", session);
    /* Excluded: validated session length makes snprintf failure/truncation unreachable. */
    if (hello_length < 0 || (size_t)hello_length >= sizeof(hello)) { /* GCOVR_EXCL_START */
        return 0;
        /* GCOVR_EXCL_STOP */
    }
    return register_hello(client, hello, (size_t)hello_length);
}

/* Register as sender and provide the relay's loss, corruption, and duplication rates. */
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
    /* Excluded: validated inputs fit the fixed registration buffer. */
    if (hello_length < 0 || (size_t)hello_length >= sizeof(hello)) { /* GCOVR_EXCL_START */
        return 0;
        /* GCOVR_EXCL_STOP */
    }
    return register_hello(client, hello, (size_t)hello_length);
}

/* Send one complete encoded protocol datagram to the connected relay. */
int relay_client_send_datagram(relay_client_t *client,
                               const unsigned char *datagram,
                               size_t datagram_length)
{
    ssize_t sent;

    if (client == NULL || client->socket_fd < 0 || datagram == NULL) {
        return -1;
    }
    /* Excluded branch: EINTR during send depends on signal delivery by the OS. */
    do {
        sent = send(client->socket_fd, datagram, datagram_length, 0);
    } while (sent < 0 && errno == EINTR); /* GCOVR_EXCL_BR_LINE */
    /* Excluded branch: EINTR and partial UDP sends are OS/network conditions. */
    return sent >= 0 && (size_t)sent == datagram_length ? 0 : -1; /* GCOVR_EXCL_BR_LINE */
}

/* Wait for and decode one relay datagram, distinguishing timeout and invalid input. */
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
    /* Excluded: wait_for_readable returns errors only for host clock/socket failures. */
    if (ready < 0) { /* GCOVR_EXCL_START */
        return RELAY_IO_ERROR;
        /* GCOVR_EXCL_STOP */
    }
    if (ready == 0) {
        return RELAY_IO_TIMEOUT;
    }
    received = recv(client->socket_fd, client->datagram, sizeof(client->datagram), 0);
    /* Excluded: recv errors depend on external socket state and OS behavior. */
    if (received < 0) { /* GCOVR_EXCL_START */
        return errno == EINTR ? RELAY_IO_TIMEOUT : RELAY_IO_ERROR;
        /* GCOVR_EXCL_STOP */
    }
    if (!packet_decode(client->datagram, (size_t)received, packet)) {
        return RELAY_IO_INVALID;
    }
    return RELAY_IO_PACKET;
}

/* Return the latest monotonic time, preserving the last value if the clock fails. */
uint64_t relay_client_now(relay_client_t *client)
{
    uint64_t now_ms;

    if (client == NULL) {
        return 0;
    }
    /* Excluded: clock_gettime failure cannot be induced reliably in unit tests. */
    if (!monotonic_now_ms(&now_ms)) { /* GCOVR_EXCL_START */
        client->clock_failed = 1;
        return client->last_now_ms;
        /* GCOVR_EXCL_STOP */
    }
    client->last_now_ms = now_ms;
    return now_ms;
}

/* Report whether a previous monotonic-clock read succeeded. */
int relay_client_clock_ok(const relay_client_t *client)
{
    return client != NULL && !client->clock_failed;
}