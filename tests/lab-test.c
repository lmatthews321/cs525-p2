#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern void __gcov_reset(void);
extern void __gcov_dump(void);

#include "harness/unity.h"
#include "../src/packet.h"
#include "../src/receiver_gbn.h"
#include "../src/receiver_io.h"
#include "../src/relay_io.h"
#include "../src/sender_gbn.h"
#include "../src/sender_io.h"

enum {
    RECEIVER_IO_TEST_NORMAL = 0,
    RECEIVER_IO_TEST_EXPIRED = 1,
    RECEIVER_IO_TEST_SEQUENCE_OVERFLOW = 2
};

enum {
    SENDER_IO_TEST_NORMAL = 0,
    SENDER_IO_TEST_DISARM_TIMER_AFTER_FIN = 1,
    SENDER_IO_TEST_NONEMPTY_ACK = 2,
    SENDER_IO_TEST_DATA_LIMIT = 3,
    SENDER_IO_TEST_FIN_SEQUENCE_LIMIT = 4
};

extern sender_status_t sender_test_queue_fin(sender_state_t *state,
                                              sender_action_t *action);
extern void receiver_io_test_set_mode(int mode);
extern void sender_io_test_set_mode(int mode);
extern int receiver_io_test_rejects_invalid_packet(void);
extern int sender_io_test_rejects_invalid_action(void);
extern const char *sender_io_test_input_path(void);

#define TEST_CHANNEL_CAPACITY 256U

typedef struct {
    unsigned char bytes[PACKET_MAX_DATAGRAM_SIZE];
    size_t length;
    int to_receiver;
} test_datagram_t;

typedef struct {
    test_datagram_t datagrams[TEST_CHANNEL_CAPACITY];
    size_t head;
    size_t count;
    uint64_t random_state;
    int impaired;
} test_channel_t;

/* Advance the deterministic test PRNG and return its next 32-bit value. */
static uint32_t test_random_next(test_channel_t *channel)
{
    uint64_t value = channel->random_state;

    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    channel->random_state = value;
    return (uint32_t)((value * UINT64_C(2685821657736338717)) >> 32);
}

/* Append one encoded datagram and its direction to the simulated network queue. */
static int test_channel_enqueue(test_channel_t *channel,
                                const unsigned char *datagram,
                                size_t datagram_length,
                                int to_receiver)
{
    size_t tail;
    test_datagram_t *message;

    if (channel->count >= TEST_CHANNEL_CAPACITY ||
        datagram_length > PACKET_MAX_DATAGRAM_SIZE) {
        return 0;
    }
    tail = (channel->head + channel->count) % TEST_CHANNEL_CAPACITY;
    message = &channel->datagrams[tail];
    memcpy(message->bytes, datagram, datagram_length);
    message->length = datagram_length;
    message->to_receiver = to_receiver;
    channel->count++;
    return 1;
}

/* Encode a packet and optionally drop, corrupt, or duplicate it in transit. */
static int test_channel_send_packet(test_channel_t *channel,
                                    const packet_t *packet,
                                    int to_receiver)
{
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length;
    int duplicate;

    if (!packet_encode(packet, datagram, sizeof(datagram), &datagram_length)) {
        return 0;
    }
    if (channel->impaired && test_random_next(channel) % 100U < 20U) {
        return 1;
    }
    if (channel->impaired && test_random_next(channel) % 100U < 20U) {
        size_t byte_index = test_random_next(channel) % datagram_length;
        unsigned char bit = (unsigned char)(1U << (test_random_next(channel) % 8U));
        datagram[byte_index] ^= bit;
    }
    duplicate = channel->impaired && test_random_next(channel) % 100U < 20U;
    if (!test_channel_enqueue(channel, datagram, datagram_length, to_receiver)) {
        return 0;
    }
    return !duplicate ||
           test_channel_enqueue(channel, datagram, datagram_length, to_receiver);
}

/* Send each packet from a sender action into the simulated channel. */
static int test_channel_send_action(test_channel_t *channel,
                                    const sender_action_t *action)
{
    size_t index;

    for (index = 0; index < action->send_count; index++) {
        if (!test_channel_send_packet(channel, &action->packets[index], 1)) {
            return 0;
        }
    }
    return 1;
}

/* Run a complete in-memory sender/receiver transfer, optionally with impairments. */
static int test_run_file_transfer(FILE *input,
                                  uint64_t seed,
                                  int impaired,
                                  unsigned char *delivered,
                                  size_t delivered_capacity,
                                  size_t *delivered_length,
                                  size_t *delivered_packets)
{
    test_channel_t channel = {0};
    sender_state_t sender;
    receiver_state_t receiver;
    uint64_t now_ms = 0;
    int eof = 0;
    size_t step;

    channel.random_state = seed == 0 ? UINT64_C(1) : seed;
    channel.impaired = impaired;
    *delivered_length = 0;
    *delivered_packets = 0;
    if (!sender_state_init(&sender, 4, 20)) {
        return 0;
    }
    receiver_state_init(&receiver, 0);

    for (step = 0; step < 200000; step++) {
        while (!eof && sender_can_accept_data(&sender)) {
            unsigned char payload[PACKET_MAX_PAYLOAD];
            size_t payload_length = fread(payload, 1, sizeof(payload), input);
            sender_action_t action;

            if (ferror(input)) {
                return 0;
            }
            if (payload_length > 0) {
                if (sender_on_data(&sender, payload, payload_length,
                                   now_ms, &action) != SENDER_ACTIVE ||
                    !test_channel_send_action(&channel, &action)) {
                    return 0;
                }
            }
            if (feof(input)) {
                eof = 1;
                if (sender_on_eof(&sender, now_ms, &action) == SENDER_ERROR ||
                    !test_channel_send_action(&channel, &action)) {
                    return 0;
                }
                break;
            }
            if (payload_length == 0) {
                return 0;
            }
        }

        if (channel.count == 0) {
            sender_action_t action;

            if (sender.completed) {
                return 1;
            }
            if (!sender.timer_armed) {
                return 0;
            }
            now_ms = sender.timer_due_ms;
            if (sender_on_timeout(&sender, now_ms, &action) != SENDER_ACTIVE ||
                !test_channel_send_action(&channel, &action)) {
                return 0;
            }
            continue;
        }

        {
            test_datagram_t message = channel.datagrams[channel.head];
            packet_t packet;

            channel.head = (channel.head + 1) % TEST_CHANNEL_CAPACITY;
            channel.count--;
            if (!packet_decode(message.bytes, message.length, &packet)) {
                continue;
            }
            if (message.to_receiver) {
                receiver_action_t action;
                receiver_status_t status = receiver_on_packet(
                    &receiver, &packet, now_ms, &action);

                if (status == RECEIVER_ERROR) {
                    return 0;
                }
                if (action.deliver_payload) {
                    if (action.payload_length > delivered_capacity - *delivered_length) {
                        return 0;
                    }
                    memcpy(delivered + *delivered_length, action.payload,
                           action.payload_length);
                    *delivered_length += action.payload_length;
                    (*delivered_packets)++;
                }
                if (action.send_ack) {
                    packet_t ack = {0};
                    ack.type = PACKET_ACK;
                    ack.seq = action.ack_sequence;
                    if (!test_channel_send_packet(&channel, &ack, 0)) {
                        return 0;
                    }
                }
            } else if (packet.type == PACKET_ACK) {
                sender_action_t action;
                sender_status_t status = sender_on_ack(
                    &sender, packet.seq, now_ms, &action);

                if (status == SENDER_ERROR ||
                    !test_channel_send_action(&channel, &action)) {
                    return 0;
                }
            }
        }
    }
    return 0;
}

/* Wrap bytes in a temporary input file and run the simulated transfer. */
static int test_simulate_bytes(const unsigned char *bytes,
                               size_t length,
                               uint64_t seed,
                               int impaired,
                               unsigned char *delivered,
                               size_t delivered_capacity,
                               size_t *delivered_length,
                               size_t *delivered_packets)
{
    FILE *input = tmpfile();
    int result;

    if (input == NULL || (length > 0 && fwrite(bytes, 1, length, input) != length)) {
        if (input != NULL) {
            (void)fclose(input);
        }
        return 0;
    }
    rewind(input);
    result = test_run_file_transfer(input, seed, impaired, delivered,
                                    delivered_capacity, delivered_length,
                                    delivered_packets);
    if (fclose(input) != 0) {
        return 0;
    }
    return result;
}

/* Compare IPv4 socket addresses by host and port. */
static int test_sockaddr_equal(const struct sockaddr_in *left,
                               const struct sockaddr_in *right)
{
    return left->sin_addr.s_addr == right->sin_addr.s_addr &&
           left->sin_port == right->sin_port;
}

/* Emulate the relay, including invalid packets and a deliberately dropped DATA ACK. */
static int test_relay_process(int socket_fd)
{
    struct sockaddr_in sender_address = {0};
    struct sockaddr_in receiver_address = {0};
    socklen_t sender_length = sizeof(sender_address);
    socklen_t receiver_length = sizeof(receiver_address);
    int have_sender = 0;
    int have_receiver = 0;
    int have_pending = 0;
    int dropped_data_ack = 0;
    int fin_seen = 0;
    unsigned char pending[PACKET_MAX_DATAGRAM_SIZE];
    size_t pending_length = 0;
    unsigned int iteration;

    for (iteration = 0; iteration < 1000; iteration++) {
        struct pollfd descriptor = {socket_fd, POLLIN, 0};
        unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE + 1U];
        struct sockaddr_in source = {0};
        socklen_t source_length = sizeof(source);
        ssize_t received;

        if (poll(&descriptor, 1, 10000) <= 0) {
            return 1;
        }
        received = recvfrom(socket_fd, datagram, sizeof(datagram), 0,
                            (struct sockaddr *)&source, &source_length);
        if (received < 0) {
            return 1;
        }
        if ((size_t)received >= 6 && memcmp(datagram, "HELLO ", 6) == 0) {
            const char *hello = (const char *)datagram;
            const char *terminator = memchr(datagram, '\0', (size_t)received);
            size_t hello_length = terminator == NULL ? (size_t)received
                                                     : (size_t)(terminator - hello);
            int is_receiver = hello_length >= 5 &&
                              memcmp(datagram + hello_length - 5, " recv", 5) == 0;
            const char reply[] = "OK";

            if (is_receiver) {
                receiver_address = source;
                receiver_length = source_length;
                have_receiver = 1;
            } else {
                sender_address = source;
                sender_length = source_length;
                have_sender = 1;
            }
            if (sendto(socket_fd, reply, sizeof(reply) - 1, 0,
                       (struct sockaddr *)&source, source_length) !=
                (ssize_t)(sizeof(reply) - 1)) {
                return 1;
            }
            if (have_sender && have_receiver) {
                const unsigned char invalid_datagram[] = {0xff};
                packet_t unexpected_packet = {0};
                unsigned char unexpected_datagram[PACKET_MAX_DATAGRAM_SIZE];
                size_t unexpected_length;

                unexpected_packet.type = PACKET_DATA;
                unexpected_packet.payload_length = 1;
                unexpected_packet.payload[0] = 'u';
                if (!packet_encode(&unexpected_packet, unexpected_datagram,
                                   sizeof(unexpected_datagram), &unexpected_length) ||
                    sendto(socket_fd, unexpected_datagram, unexpected_length, 0,
                           (struct sockaddr *)&sender_address, sender_length) !=
                        (ssize_t)unexpected_length) {
                    return 1;
                }
                (void)sendto(socket_fd, invalid_datagram, sizeof(invalid_datagram), 0,
                             (struct sockaddr *)&sender_address, sender_length);
                (void)sendto(socket_fd, invalid_datagram, sizeof(invalid_datagram), 0,
                             (struct sockaddr *)&receiver_address, receiver_length);
                {
                    packet_t ignored_ack = {0};
                    unsigned char ack_datagram[PACKET_HEADER_SIZE];
                    size_t ack_length;

                    ignored_ack.type = PACKET_ACK;
                    if (!packet_encode(&ignored_ack, ack_datagram,
                                       sizeof(ack_datagram), &ack_length) ||
                        sendto(socket_fd, ack_datagram, ack_length, 0,
                               (struct sockaddr *)&receiver_address,
                               receiver_length) != (ssize_t)ack_length) {
                        return 1;
                    }
                }
                if (have_pending) {
                    if (sendto(socket_fd, pending, pending_length, 0,
                               (struct sockaddr *)&receiver_address, receiver_length) !=
                        (ssize_t)pending_length) {
                        return 1;
                    }
                    have_pending = 0;
                }
            }
            continue;
        }

        if (have_sender && test_sockaddr_equal(&source, &sender_address)) {
            packet_t packet;

            if (packet_decode(datagram, (size_t)received, &packet) &&
                packet.type == PACKET_FIN) {
                fin_seen = 1;
            }
            if (!have_receiver) {
                memcpy(pending, datagram, (size_t)received);
                pending_length = (size_t)received;
                have_pending = 1;
                continue;
            }
            if (sendto(socket_fd, datagram, (size_t)received, 0,
                       (struct sockaddr *)&receiver_address, receiver_length) != received) {
                return 1;
            }
        } else if (have_receiver && test_sockaddr_equal(&source, &receiver_address)) {
            packet_t packet;
            int is_fin_ack = fin_seen &&
                             packet_decode(datagram, (size_t)received, &packet) &&
                             packet.type == PACKET_ACK;

            if (!dropped_data_ack && packet_decode(datagram, (size_t)received, &packet) &&
                packet.type == PACKET_ACK && packet.seq == 1) {
                dropped_data_ack = 1;
                continue;
            }
            if (have_sender && sendto(socket_fd, datagram, (size_t)received, 0,
                                      (struct sockaddr *)&sender_address,
                                      sender_length) != received) {
                return 1;
            }
            if (is_fin_ack) {
                return 0;
            }
        }
    }
    return 1;
}

/* Reply to a chosen number of registrations and optionally send a follow-up packet. */
static int test_registration_server(int socket_fd,
                                    const char *reply,
                                    unsigned int registration_count)
{
    unsigned int attempt;

    for (attempt = 0; attempt < registration_count; attempt++) {
        unsigned char request[128];
        struct sockaddr_in source = {0};
        socklen_t source_length = sizeof(source);
        ssize_t received = recvfrom(socket_fd, request, sizeof(request), 0,
                                    (struct sockaddr *)&source, &source_length);

        if (received < 0) {
            return 1;
        }
        {
            const char *wire_reply = reply;
            if (reply != NULL &&
                (strcmp(reply, "OK_PACKET") == 0 || strcmp(reply, "OK_DATA_MAX") == 0)) {
                wire_reply = "OK";
            }
            if (wire_reply != NULL &&
                sendto(socket_fd, wire_reply, strlen(wire_reply), 0,
                   (struct sockaddr *)&source, source_length) !=
                    (ssize_t)strlen(wire_reply)) {
                return 1;
            }
        }
        if (reply != NULL && strcmp(reply, "OK_PACKET") == 0) {
            packet_t packet = {0};
            unsigned char datagram[PACKET_HEADER_SIZE];
            size_t datagram_length;

            packet.type = PACKET_ACK;
            if (!packet_encode(&packet, datagram, sizeof(datagram), &datagram_length) ||
                sendto(socket_fd, datagram, datagram_length, 0,
                       (struct sockaddr *)&source, source_length) !=
                    (ssize_t)datagram_length) {
                return 1;
            }
        } else if (reply != NULL && strcmp(reply, "OK_DATA_MAX") == 0) {
            packet_t packet = {0};
            unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
            size_t datagram_length;

            packet.type = PACKET_DATA;
            packet.seq = UINT32_MAX;
            packet.payload_length = 1;
            packet.payload[0] = 'x';
            if (!packet_encode(&packet, datagram, sizeof(datagram), &datagram_length) ||
                sendto(socket_fd, datagram, datagram_length, 0,
                       (struct sockaddr *)&source, source_length) !=
                    (ssize_t)datagram_length) {
                return 1;
            }
        }
    }
    return 0;
}

/* Accept a sender registration and discard DATA until its retry limit is reached. */
static int test_drop_ack_server(int socket_fd)
{
    struct sockaddr_in sender_address = {0};
    socklen_t sender_length = sizeof(sender_address);
    unsigned char hello[128];
    ssize_t received;
    const char reply[] = "OK";
    unsigned int data_count = 0;

    received = recvfrom(socket_fd, hello, sizeof(hello), 0,
                        (struct sockaddr *)&sender_address, &sender_length);
    if (received < 0 ||
        sendto(socket_fd, reply, sizeof(reply) - 1, 0,
               (struct sockaddr *)&sender_address, sender_length) !=
            (ssize_t)(sizeof(reply) - 1)) {
        return 1;
    }

    while (data_count < SENDER_MAX_TIMEOUTS) {
        struct pollfd descriptor = {socket_fd, POLLIN, 0};
        unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
        packet_t packet;

        if (poll(&descriptor, 1, 5000) <= 0) {
            return 1;
        }
        received = recvfrom(socket_fd, datagram, sizeof(datagram), 0,
                            (struct sockaddr *)&sender_address, &sender_length);
        if (received < 0) {
            return 1;
        }
        if (packet_decode(datagram, (size_t)received, &packet) &&
            packet.type == PACKET_DATA) {
            data_count++;
        }
    }
    return 0;
}

/* Run a local UDP relay scenario and check the requested registration outcome. */
static int test_registration_result(const char *reply,
                                    unsigned int registration_count,
                                    int expected_result,
                                    int operation,
                                    uint32_t window_size)
{
    struct sockaddr_in relay_address = {0};
    socklen_t relay_address_length = sizeof(relay_address);
    relay_client_t client = {-1, {0}, 0, 0};
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int server_status;
    int registration_result;
    pid_t server_pid;
    packet_t packet;

    if (socket_fd < 0) {
        return 0;
    }
    relay_address.sin_family = AF_INET;
    relay_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(socket_fd, (struct sockaddr *)&relay_address, sizeof(relay_address)) != 0 ||
        getsockname(socket_fd, (struct sockaddr *)&relay_address,
                    &relay_address_length) != 0) {
        (void)close(socket_fd);
        return 0;
    }
    server_pid = fork();
    if (server_pid < 0) {
        (void)close(socket_fd);
        return 0;
    }
    if (server_pid == 0) {
        int status;
        __gcov_reset();
        status = test_registration_server(socket_fd, reply, registration_count);
        __gcov_dump();
        _exit(status);
    }
    (void)close(socket_fd);
    if (!relay_client_open(&client, "127.0.0.1", ntohs(relay_address.sin_port))) {
        (void)waitpid(server_pid, &server_status, 0);
        return 0;
    }
    if (relay_client_receive_packet(&client, 0, &packet) != RELAY_IO_TIMEOUT) {
        relay_client_close(&client);
        return 0;
    }
    if (operation == 1) {
        relay_client_close(&client);
        registration_result = sender_send_file(
            "coveragetest", "127.0.0.1",
            ntohs(relay_address.sin_port),
            sender_io_test_input_path(),
            window_size, 1, 0.5, 0.5, 0.5);
    } else if (operation == 2) {
        relay_client_close(&client);
        registration_result = receiver_receive_file("coveragetest", "127.0.0.1",
                                                     ntohs(relay_address.sin_port), "/dev/null");
    } else {
        registration_result = relay_client_register_receiver(&client, "coveragetest");
        if (registration_result && reply != NULL && strcmp(reply, "OK_PACKET") == 0 &&
            relay_client_receive_packet(&client, UINT64_MAX, &packet) != RELAY_IO_PACKET) {
            relay_client_close(&client);
            return 0;
        }
        relay_client_close(&client);
    }
    if (waitpid(server_pid, &server_status, 0) != server_pid ||
        !WIFEXITED(server_status) || WEXITSTATUS(server_status) != 0) {
        return 0;
    }
    return registration_result == expected_result;
}

/* Verify packet checksum/encode/decode APIs reject invalid pointers and fields. */
static void test_packet_api_rejects_invalid_arguments(void)
{
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE + 1U] = {0};
    size_t datagram_length = 0;

    TEST_ASSERT_EQUAL_INT(0, packet_checksum(NULL, 1));
    TEST_ASSERT_EQUAL_INT(0xffff, packet_checksum(NULL, 0));
    TEST_ASSERT_FALSE(packet_encode(NULL, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_FALSE(packet_encode(&packet, NULL, sizeof(datagram), &datagram_length));
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, sizeof(datagram), NULL));

    packet.type = (packet_type_t)3;
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    packet.type = PACKET_ACK;
    packet.payload_length = 1;
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    packet.payload_length = PACKET_MAX_PAYLOAD + 1U;
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    packet.payload_length = 0;
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, PACKET_HEADER_SIZE - 1,
                                   &datagram_length));
    TEST_ASSERT_FALSE(packet_decode(NULL, 0, &decoded));
    TEST_ASSERT_FALSE(packet_decode(datagram, PACKET_HEADER_SIZE, NULL));

    packet.type = PACKET_ACK;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    datagram[8] = 0x04;
    datagram[9] = 0x01;
    TEST_ASSERT_FALSE(packet_decode(datagram, sizeof(datagram), &decoded));

    packet.type = PACKET_DATA;
    packet.payload_length = 1;
    packet.payload[0] = 'x';
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    datagram[0] = PACKET_ACK;
    datagram[2] = 0;
    datagram[3] = 0;
    {
        uint16_t checksum = htons(packet_checksum(datagram, datagram_length));
        memcpy(datagram + 2, &checksum, sizeof(checksum));
    }
    TEST_ASSERT_FALSE(packet_decode(datagram, datagram_length, &decoded));
}

/* Cover receiver argument/state guards, sequence overflow, and deadline saturation. */
static void test_receiver_state_rejects_invalid_transitions_and_saturates(void)
{
    receiver_state_t state;
    receiver_action_t action;
    packet_t packet = {0};

    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid(NULL));
    receiver_state_init(NULL, 0);
    TEST_ASSERT_EQUAL_UINT64(0, receiver_next_deadline(NULL));
    receiver_state_init(&state, UINT64_MAX - RECEIVER_IDLE_TIMEOUT_MS + 1);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, receiver_next_deadline(&state));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(NULL, &packet, 0, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, NULL, 0, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 0, NULL));
    packet.type = (packet_type_t)3;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 0, &action));
    packet.type = PACKET_ACK;
    packet.payload_length = 1;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 0, &action));
    packet.type = PACKET_DATA;
    packet.payload_length = PACKET_MAX_PAYLOAD + 1U;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 0, &action));

    packet.payload_length = 0;
    receiver_state_init(&state, 10);
    packet.type = PACKET_ACK;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 20, &action));
    TEST_ASSERT_EQUAL_UINT64(10, state.last_valid_ms);
    packet.type = PACKET_DATA;
    packet.seq = state.expected + 2;
    packet.payload_length = 0;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 25, &action));
    TEST_ASSERT_FALSE(action.deliver_payload);
    packet.type = PACKET_FIN;
    packet.seq = 1;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 30, &action));
    TEST_ASSERT_TRUE(action.send_ack);

    receiver_state_init(&state, 50);
    packet.type = PACKET_DATA;
    packet.seq = 0;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 51, &action));
    TEST_ASSERT_FALSE(action.deliver_payload);

    state.expected = UINT32_MAX;
    packet.type = PACKET_DATA;
    packet.seq = UINT32_MAX;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 40, &action));
    packet.type = PACKET_FIN;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 40, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR, receiver_on_timeout(NULL, 0, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR, receiver_on_timeout(&state, 0, NULL));
    receiver_state_init(&state, 100);
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_timeout(&state, 99, &action));
}

/* Cover sender argument/state guards, packet limits, and timer overflow behavior. */
static void test_sender_state_rejects_invalid_transitions_and_saturates(void)
{
    sender_state_t state;
    sender_action_t action;
    unsigned char payload = 'x';

    TEST_ASSERT_FALSE(sender_state_init(NULL, 1, 1));
    TEST_ASSERT_FALSE(sender_can_accept_data(NULL));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_data(NULL, &payload, 1, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_ack(NULL, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(NULL, 0, &action));
    TEST_ASSERT_TRUE(sender_state_init(&state, 1, 10));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_timeout(&state, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_data(&state, NULL, 1, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_data(&state, &payload, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR,
                          sender_on_data(&state, &payload, PACKET_MAX_PAYLOAD + 1U,
                                         0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_data(&state, &payload, 1, 0, NULL));

    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&state, &payload, 1, UINT64_MAX - 2, &action));
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, state.timer_due_ms);
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_eof(NULL, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_eof(&state, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_ack(&state, 0, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(&state, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&state, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_ack(&state, state.next + 1, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_timeout(&state, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(&state, 0, NULL));

    state.base = SENDER_MAX_DATA_PACKETS;
    state.next = SENDER_MAX_DATA_PACKETS;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR,
                          sender_on_data(&state, &payload, 1, 0, &action));
    state.next = UINT32_MAX;
    state.base = UINT32_MAX;
    state.timer_armed = 0;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_eof(&state, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&state, SENDER_MAX_WINDOW, 1));
    state.failed = 1;
    TEST_ASSERT_FALSE(sender_can_accept_data(&state));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_eof(&state, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_ack(&state, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(&state, 0, &action));
    state.failed = 0;
    state.eof = 1;
    TEST_ASSERT_FALSE(sender_can_accept_data(&state));
    state.eof = 0;
    state.fin_sent = 1;
    TEST_ASSERT_FALSE(sender_can_accept_data(&state));
    state.fin_sent = 0;

    state.next = SENDER_MAX_WINDOW + 1;
    state.timer_armed = 1;
    state.timer_due_ms = 0;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(&state, 1, &action));
    TEST_ASSERT_TRUE(state.failed);

    TEST_ASSERT_TRUE(sender_state_init(&state, 1, 1));
    state.eof = 1;
    state.fin_sent = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_eof(&state, 0, &action));
    state.fin_sent = 0;
    state.next = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_eof(&state, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&state, 1, 1));
    state.base = UINT32_MAX - 1;
    state.next = UINT32_MAX;
    state.eof = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR,
                          sender_on_ack(&state, UINT32_MAX, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&state, 2, 1));
    state.base = 0;
    state.next = 2;
    state.fin_sent = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&state, 1, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&state, 1, 1));
    memset(&action, 0, sizeof(action));
    action.send_count = SENDER_MAX_WINDOW;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_test_queue_fin(&state, &action));
    TEST_ASSERT_TRUE(state.failed);

    state.failed = 0;
    state.completed = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_DONE, sender_on_eof(&state, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_DONE, sender_on_ack(&state, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_DONE, sender_on_timeout(&state, 0, &action));
}

/* Create temporary files and processes to test an end-to-end UDP file transfer. */
static void test_run_network_transfer(const unsigned char *payload, size_t payload_length)
{
    unsigned char received[PACKET_MAX_PAYLOAD];
    char input_path[] = "/tmp/cs525-input-XXXXXX";
    char output_path[] = "/tmp/cs525-output-XXXXXX";
    struct sockaddr_in relay_address = {0};
    socklen_t relay_address_length = sizeof(relay_address);
    int relay_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int input_fd;
    int output_fd;
    pid_t relay_pid;
    pid_t sender_pid;
    int relay_status;
    int sender_status;
    int receiver_status;
    int input_ok;
    FILE *output;
    size_t received_length;

    TEST_ASSERT_TRUE(relay_fd >= 0);
    relay_address.sin_family = AF_INET;
    relay_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    TEST_ASSERT_EQUAL_INT(0, bind(relay_fd, (struct sockaddr *)&relay_address,
                                  sizeof(relay_address)));
    TEST_ASSERT_EQUAL_INT(0, getsockname(relay_fd, (struct sockaddr *)&relay_address,
                                         &relay_address_length));
    input_fd = mkstemp(input_path);
    output_fd = mkstemp(output_path);
    TEST_ASSERT_TRUE(input_fd >= 0 && output_fd >= 0);
    input_ok = payload_length == 0 ||
               write(input_fd, payload, payload_length) == (ssize_t)payload_length;
    TEST_ASSERT_EQUAL_INT(0, close(input_fd));
    TEST_ASSERT_EQUAL_INT(0, close(output_fd));

    relay_pid = fork();
    TEST_ASSERT_TRUE(relay_pid >= 0);
    if (relay_pid == 0) {
        int status;
        __gcov_reset();
        status = test_relay_process(relay_fd);
        __gcov_dump();
        _exit(status);
    }
    TEST_ASSERT_EQUAL_INT(0, close(relay_fd));
    sender_pid = fork();
    TEST_ASSERT_TRUE(sender_pid >= 0);
    if (sender_pid == 0) {
        int status;
        __gcov_reset();
        status = sender_send_file("coveragetest", "127.0.0.1",
                                  ntohs(relay_address.sin_port), input_path,
                                  1, 20, 0.0, 0.0, 0.0);
        __gcov_dump();
        _exit(status);
    }

    receiver_status = receiver_receive_file("coveragetest", "127.0.0.1",
                                             ntohs(relay_address.sin_port), output_path);
    TEST_ASSERT_EQUAL_INT(sender_pid, waitpid(sender_pid, &sender_status, 0));
    TEST_ASSERT_EQUAL_INT(relay_pid, waitpid(relay_pid, &relay_status, 0));
    TEST_ASSERT_TRUE(input_ok);
    TEST_ASSERT_EQUAL_INT(0, receiver_status);
    TEST_ASSERT_TRUE(WIFEXITED(sender_status));
    TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(sender_status));
    TEST_ASSERT_TRUE(WIFEXITED(relay_status));
    TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(relay_status));

    output = fopen(output_path, "rb");
    TEST_ASSERT_NOT_NULL(output);
    received_length = fread(received, 1, sizeof(received), output);
    TEST_ASSERT_EQUAL_INT(0, fclose(output));
    TEST_ASSERT_EQUAL_UINT64(payload_length, received_length);
    TEST_ASSERT_TRUE(payload_length == 0 ||
                     memcmp(payload, received, payload_length) == 0);
    TEST_ASSERT_EQUAL_INT(0, unlink(input_path));
    TEST_ASSERT_EQUAL_INT(0, unlink(output_path));
}

/* Verify a nonempty file transfers through the local test relay. */
void test_network_transfer_through_loopback_relay(void)
{
    const unsigned char payload[] = "loopback coverage";

    test_run_network_transfer(payload, sizeof(payload));
}

/* Verify an empty file completes without sending a DATA packet. */
void test_empty_network_transfer_sends_only_fin(void)
{
    test_run_network_transfer(NULL, 0);
}

/* Verify a full-sized payload transfers correctly through the network path. */
void test_exact_payload_network_transfer(void)
{
    unsigned char payload[PACKET_MAX_PAYLOAD];
    size_t index;

    for (index = 0; index < sizeof(payload); index++) {
        payload[index] = (unsigned char)(index * 29U);
    }
    test_run_network_transfer(payload, sizeof(payload));
}

/* Unity fixture hook: these tests do not require per-test setup. */
void setUp(void) {}
/* Unity fixture hook: tests own and release their temporary resources. */
void tearDown(void) {}

/* Check the checksum against the published RFC 1071 example. */
void test_checksum_matches_rfc1071_example(void)
{
    const unsigned char example[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    TEST_ASSERT_EQUAL_INT(0x220d, packet_checksum(example, sizeof(example)));
}

/* Verify odd-length checksums and detection of a modified datagram byte. */
void test_checksum_handles_odd_length_and_detects_a_flipped_bit(void)
{
    const unsigned char odd_bytes[] = {0x01, 0x02, 0x03};
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length = 0;

    TEST_ASSERT_EQUAL_INT(0xfbfd, packet_checksum(odd_bytes, sizeof(odd_bytes)));
    packet.type = PACKET_DATA;
    packet.payload_length = sizeof(odd_bytes);
    memcpy(packet.payload, odd_bytes, sizeof(odd_bytes));
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_EQUAL_INT(0, packet_checksum(datagram, datagram_length));
    datagram[PACKET_HEADER_SIZE + 1] ^= 1;
    TEST_ASSERT_NOT_EQUAL(0, packet_checksum(datagram, datagram_length));
    TEST_ASSERT_FALSE(packet_decode(datagram, datagram_length, &decoded));
}

/* Verify session names accept only the allowed characters and length. */
void test_session_name_validation(void)
{
    TEST_ASSERT_TRUE(receiver_session_valid("jdoe-1"));
    TEST_ASSERT_TRUE(receiver_session_valid("abcdefghijklmnopqrstuvwxyz123456"));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid(""));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid("Bad_Name"));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid("jdoe{"));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid("abcdefghijklmnopqrstuvwxyz1234567"));
}

/* Verify packet fields encode in network order and decode back with checksum checks. */
void test_packet_wire_format_round_trips_and_checks_checksum(void)
{
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length = 0;

    packet.type = PACKET_DATA;
    packet.seq = UINT32_C(0x01020304);
    packet.payload_length = 2;
    packet.payload[0] = 0xaa;
    packet.payload[1] = 0xbb;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_EQUAL_UINT64(PACKET_HEADER_SIZE + 2, datagram_length);
    TEST_ASSERT_EQUAL_INT(0, datagram[0]);
    TEST_ASSERT_EQUAL_INT(0, datagram[1]);
    TEST_ASSERT_EQUAL_INT(1, datagram[4]);
    TEST_ASSERT_EQUAL_INT(2, datagram[5]);
    TEST_ASSERT_EQUAL_INT(3, datagram[6]);
    TEST_ASSERT_EQUAL_INT(4, datagram[7]);
    TEST_ASSERT_EQUAL_INT(0, datagram[8]);
    TEST_ASSERT_EQUAL_INT(2, datagram[9]);
    TEST_ASSERT_TRUE(packet_decode(datagram, datagram_length, &decoded));
    TEST_ASSERT_EQUAL_INT(PACKET_DATA, decoded.type);
    TEST_ASSERT_EQUAL_UINT64(UINT32_C(0x01020304), decoded.seq);
    TEST_ASSERT_EQUAL_UINT64(2, decoded.payload_length);
    TEST_ASSERT_EQUAL_INT(0xaa, decoded.payload[0]);
    TEST_ASSERT_EQUAL_INT(0xbb, decoded.payload[1]);

    datagram[PACKET_HEADER_SIZE] ^= 1;
    TEST_ASSERT_EQUAL_INT(0, packet_decode(datagram, datagram_length, &decoded));
}

/* Verify the decoder rejects reserved bits, extra bytes, and payloads on ACKs. */
void test_packet_decoder_rejects_invalid_header_and_size(void)
{
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE + 1] = {0};
    size_t datagram_length = 0;

    packet.type = PACKET_ACK;
    packet.seq = 5;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_EQUAL_INT(PACKET_ACK, datagram[0]);
    TEST_ASSERT_TRUE(packet_decode(datagram, datagram_length, &decoded));
    TEST_ASSERT_EQUAL_UINT64(5, decoded.seq);

    datagram[1] = 1;
    TEST_ASSERT_EQUAL_INT(0, packet_decode(datagram, datagram_length, &decoded));
    datagram[1] = 0;
    TEST_ASSERT_EQUAL_INT(0, packet_decode(datagram, datagram_length + 1, &decoded));
    packet.payload_length = 1;
    TEST_ASSERT_EQUAL_INT(0, packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
}

/* Verify short datagrams, length mismatches, and unknown packet types are rejected. */
void test_packet_decoder_rejects_short_length_mismatch_and_unknown_type(void)
{
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE] = {0};
    size_t datagram_length = 0;

    packet.type = PACKET_ACK;
    packet.seq = 9;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_FALSE(packet_decode(datagram, PACKET_HEADER_SIZE - 1, &decoded));

    datagram[9] = 1;
    TEST_ASSERT_FALSE(packet_decode(datagram, datagram_length, &decoded));
    datagram[9] = 0;
    datagram[0] = 3;
    TEST_ASSERT_FALSE(packet_decode(datagram, datagram_length, &decoded));
}

/* Verify the receiver delivers only the expected sequence and ACKs duplicates/gaps. */
void test_receiver_acknowledges_only_in_order_data(void)
{
    receiver_state_t state;
    receiver_action_t action;
    packet_t packet = {0};

    receiver_state_init(&state, 100);
    packet.type = PACKET_DATA;
    packet.seq = 2;
    packet.payload_length = 1;
    packet.payload[0] = 'C';
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 200, &action));
    TEST_ASSERT_TRUE(action.send_ack);
    TEST_ASSERT_EQUAL_UINT64(0, action.ack_sequence);
    TEST_ASSERT_FALSE(action.deliver_payload);
    TEST_ASSERT_EQUAL_UINT64(0, state.expected);

    packet.seq = 0;
    packet.payload[0] = 'A';
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 300, &action));
    TEST_ASSERT_TRUE(action.deliver_payload);
    TEST_ASSERT_EQUAL_INT('A', action.payload[0]);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);
    TEST_ASSERT_EQUAL_UINT64(1, state.expected);

    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 400, &action));
    TEST_ASSERT_FALSE(action.deliver_payload);
    TEST_ASSERT_TRUE(action.send_ack);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);

    packet.seq = 4;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 500, &action));
    TEST_ASSERT_FALSE(action.deliver_payload);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);
}

/* Verify FIN is ACKed and duplicate FINs are handled during the linger period. */
void test_receiver_fin_acknowledges_and_lingers_until_deadline(void)
{
    receiver_state_t state;
    receiver_action_t action;
    packet_t packet = {0};

    receiver_state_init(&state, 100);
    packet.type = PACKET_FIN;
    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&state, &packet, 500, &action));
    TEST_ASSERT_TRUE(action.send_ack);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);
    TEST_ASSERT_EQUAL_UINT64(2500, action.timer_due_ms);

    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&state, &packet, 1000, &action));
    TEST_ASSERT_TRUE(action.send_ack);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);
    packet.seq = 9;
    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&state, &packet, 1100, &action));
    TEST_ASSERT_FALSE(action.send_ack);
    packet.type = PACKET_DATA;
    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&state, &packet, 1200, &action));
    TEST_ASSERT_FALSE(action.send_ack);
    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_timeout(&state, 2499, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_DONE,
                          receiver_on_timeout(&state, 2500, &action));
    TEST_ASSERT_FALSE(action.timer_armed);
}

/* Verify idle timeout calculations use the supplied clock value. */
void test_receiver_idle_timeout_uses_passed_time(void)
{
    receiver_state_t state;
    receiver_action_t action;

    receiver_state_init(&state, 100);
    TEST_ASSERT_EQUAL_UINT64(100 + RECEIVER_IDLE_TIMEOUT_MS,
                             receiver_next_deadline(&state));
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_timeout(&state,
                              100 + RECEIVER_IDLE_TIMEOUT_MS - 1, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_TIMED_OUT,
                          receiver_on_timeout(&state,
                              100 + RECEIVER_IDLE_TIMEOUT_MS, &action));
}

/* Verify window capacity, cumulative ACK advancement, and duplicate ACK handling. */
void test_sender_window_full_cumulative_ack_and_duplicate_ack(void)
{
    sender_state_t sender;
    sender_action_t action;
    const unsigned char payload = 'x';
    uint32_t sequence;

    TEST_ASSERT_FALSE(sender_state_init(&sender, 0, 10));
    TEST_ASSERT_FALSE(sender_state_init(&sender, SENDER_MAX_WINDOW + 1, 10));
    TEST_ASSERT_FALSE(sender_state_init(&sender, 1, 0));
    TEST_ASSERT_TRUE(sender_state_init(&sender, 2, 10));
    TEST_ASSERT_TRUE(sender_can_accept_data(&sender));
    for (sequence = 0; sequence < 2; sequence++) {
        TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                              sender_on_data(&sender, &payload, 1, 0, &action));
    }
    TEST_ASSERT_FALSE(sender_can_accept_data(&sender));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR,
                          sender_on_data(&sender, &payload, 1, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&sender, 4, 10));
    for (sequence = 0; sequence < 4; sequence++) {
        TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                              sender_on_data(&sender, &payload, 1, 0, &action));
    }
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&sender, 0, 4, &action));
    TEST_ASSERT_EQUAL_UINT64(0, sender.base);
    TEST_ASSERT_EQUAL_UINT64(10, sender.timer_due_ms);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&sender, 3, 5, &action));
    TEST_ASSERT_EQUAL_UINT64(3, sender.base);
    TEST_ASSERT_EQUAL_UINT64(4, sender.next);
    TEST_ASSERT_EQUAL_UINT64(15, sender.timer_due_ms);
    TEST_ASSERT_TRUE(sender_can_accept_data(&sender));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&sender, 3, 8, &action));
    TEST_ASSERT_EQUAL_UINT64(3, sender.base);
    TEST_ASSERT_EQUAL_UINT64(15, sender.timer_due_ms);
}

/* Verify a timeout retransmits every packet that remains outstanding. */
void test_sender_timeout_resends_every_packet_in_the_window(void)
{
    sender_state_t sender;
    sender_action_t action;
    const unsigned char payload = 'z';
    uint32_t sequence;

    TEST_ASSERT_TRUE(sender_state_init(&sender, 3, 10));
    for (sequence = 0; sequence < 3; sequence++) {
        TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                              sender_on_data(&sender, &payload, 1, 0, &action));
    }
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_timeout(&sender, 9, &action));
    TEST_ASSERT_EQUAL_UINT64(0, action.send_count);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_timeout(&sender, 10, &action));
    TEST_ASSERT_EQUAL_UINT64(3, action.send_count);
    for (sequence = 0; sequence < 3; sequence++) {
        TEST_ASSERT_EQUAL_UINT64(sequence, action.packets[sequence].seq);
        TEST_ASSERT_EQUAL_INT(PACKET_DATA, action.packets[sequence].type);
    }
}

/* Verify retransmission recovers a dropped DATA packet and completes the transfer. */
void test_sender_retransmits_a_lost_packet_and_completes_in_memory(void)
{
    sender_state_t sender;
    receiver_state_t receiver;
    sender_action_t sender_action;
    receiver_action_t receiver_action;
    packet_t second_packet;
    packet_t retransmitted_second;
    unsigned char delivered[2] = {0};
    size_t delivered_length = 0;
    unsigned char first = 'A';
    unsigned char second = 'B';

    TEST_ASSERT_TRUE(sender_state_init(&sender, 2, 10));
    receiver_state_init(&receiver, 0);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&sender, &first, 1, 0, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(1, sender_action.send_count);
    TEST_ASSERT_EQUAL_UINT64(0, sender_action.packets[0].seq);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&sender, &second, 1, 0, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(1, sender_action.packets[0].seq);
    second_packet = sender_action.packets[0];
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_eof(&sender, 0, &sender_action));

    /* Drop sequence zero; passing sequence one produces a duplicate ACK. */
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&receiver, &second_packet, 1, &receiver_action));
    TEST_ASSERT_EQUAL_UINT64(0, receiver_action.ack_sequence);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_ack(&sender, receiver_action.ack_sequence,
                                        1, &sender_action));

    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_timeout(&sender, 10, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(2, sender_action.send_count);
    TEST_ASSERT_EQUAL_UINT64(0, sender_action.packets[0].seq);
    TEST_ASSERT_EQUAL_UINT64(1, sender_action.packets[1].seq);
    retransmitted_second = sender_action.packets[1];

    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&receiver, &sender_action.packets[0],
                                             10, &receiver_action));
    TEST_ASSERT_TRUE(receiver_action.deliver_payload);
    delivered[delivered_length++] = receiver_action.payload[0];
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_ack(&sender, receiver_action.ack_sequence,
                                        10, &sender_action));

    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&receiver, &retransmitted_second,
                                             10, &receiver_action));
    TEST_ASSERT_TRUE(receiver_action.deliver_payload);
    delivered[delivered_length++] = receiver_action.payload[0];
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_ack(&sender, receiver_action.ack_sequence,
                                        10, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(1, sender_action.send_count);
    TEST_ASSERT_EQUAL_INT(PACKET_FIN, sender_action.packets[0].type);
    TEST_ASSERT_EQUAL_UINT64(2, sender_action.packets[0].seq);

    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&receiver, &sender_action.packets[0],
                                             11, &receiver_action));
    TEST_ASSERT_EQUAL_INT(SENDER_DONE,
                          sender_on_ack(&sender, receiver_action.ack_sequence,
                                        11, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(2, delivered_length);
    TEST_ASSERT_EQUAL_INT('A', delivered[0]);
    TEST_ASSERT_EQUAL_INT('B', delivered[1]);
}

/* Verify the sender reports failure after the configured number of timeouts. */
void test_sender_stops_after_ten_expired_timers(void)
{
    sender_state_t sender;
    sender_action_t action;
    const unsigned char payload = 'x';
    uint32_t attempt;

    TEST_ASSERT_TRUE(sender_state_init(&sender, 1, 10));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&sender, &payload, 1, 0, &action));
    for (attempt = 1; attempt <= SENDER_MAX_TIMEOUTS; attempt++) {
        sender_status_t expected = attempt == SENDER_MAX_TIMEOUTS
            ? SENDER_ERROR : SENDER_ACTIVE;
        TEST_ASSERT_EQUAL_INT(expected,
                              sender_on_timeout(&sender, attempt * 10, &action));
        if (attempt < SENDER_MAX_TIMEOUTS) {
            TEST_ASSERT_EQUAL_UINT64(1, action.send_count);
        }
    }
}

/* Verify a final FIN can use the sequence following the maximum DATA packet count. */
void test_sender_allows_eof_after_maximum_data_packet_count(void)
{
    sender_state_t sender;
    sender_action_t action;

    TEST_ASSERT_TRUE(sender_state_init(&sender, 1, 10));
    sender.base = SENDER_MAX_DATA_PACKETS;
    sender.next = SENDER_MAX_DATA_PACKETS;
    TEST_ASSERT_TRUE(sender_can_accept_data(&sender));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_eof(&sender, 100, &action));
    TEST_ASSERT_EQUAL_UINT64(1, action.send_count);
    TEST_ASSERT_EQUAL_INT(PACKET_FIN, action.packets[0].type);
    TEST_ASSERT_EQUAL_UINT64(SENDER_MAX_DATA_PACKETS, action.packets[0].seq);
}

/* Verify an empty input completes successfully with no delivered DATA packets. */
void test_empty_file_transfers_without_data_packets(void)
{
    unsigned char delivered[1];
    size_t delivered_length = 99;
    size_t delivered_packets = 99;

    TEST_ASSERT_TRUE(test_simulate_bytes(NULL, 0, UINT64_C(41), 0,
                                         delivered, sizeof(delivered),
                                         &delivered_length, &delivered_packets));
    TEST_ASSERT_EQUAL_UINT64(0, delivered_length);
    TEST_ASSERT_EQUAL_UINT64(0, delivered_packets);
}

/* Verify exact payload multiples do not produce an extra empty DATA packet. */
void test_exact_multiple_of_packet_payload_transfers_without_empty_data(void)
{
    unsigned char input[2 * PACKET_MAX_PAYLOAD];
    unsigned char delivered[sizeof(input)];
    size_t delivered_length;
    size_t delivered_packets;
    size_t index;

    for (index = 0; index < sizeof(input); index++) {
        input[index] = (unsigned char)(index * 17U);
    }
    TEST_ASSERT_TRUE(test_simulate_bytes(input, sizeof(input), UINT64_C(53), 0,
                                         delivered, sizeof(delivered),
                                         &delivered_length, &delivered_packets));
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), delivered_length);
    TEST_ASSERT_EQUAL_UINT64(2, delivered_packets);
    TEST_ASSERT_TRUE(memcmp(input, delivered, sizeof(input)) == 0);
}

/* Verify repeatable loss, corruption, and duplication in both directions are recovered. */
void test_complete_transfer_survives_deterministic_bidirectional_impairment(void)
{
    unsigned char input[16384];
    unsigned char delivered[sizeof(input)];
    const uint64_t seeds[] = {UINT64_C(7), UINT64_C(107), UINT64_C(525)};
    size_t delivered_length;
    size_t delivered_packets;
    size_t index;
    size_t seed_index;

    for (index = 0; index < sizeof(input); index++) {
        input[index] = (unsigned char)((index * 37U + index / 7U) & 0xffU);
    }
    for (seed_index = 0; seed_index < sizeof(seeds) / sizeof(seeds[0]); seed_index++) {
        TEST_ASSERT_TRUE(test_simulate_bytes(input, sizeof(input), seeds[seed_index], 1,
                                             delivered, sizeof(delivered),
                                             &delivered_length, &delivered_packets));
        TEST_ASSERT_EQUAL_UINT64(sizeof(input), delivered_length);
        TEST_ASSERT_TRUE(memcmp(input, delivered, sizeof(input)) == 0);
    }
}

/* Verify public sender, receiver, and relay APIs handle invalid arguments safely. */
void test_network_api_rejects_invalid_arguments(void)
{
    relay_client_t client = {-1, {0}, 0, 0};
    unsigned char datagram[PACKET_HEADER_SIZE] = {0};
    packet_t packet;

    TEST_ASSERT_EQUAL_INT(1, sender_send_file(NULL, "relay", 9000, "input", 1, 1,
                                               0.0, 0.0, 0.0));
    TEST_ASSERT_EQUAL_INT(1, receiver_receive_file(NULL, "relay", 9000, "output"));
    TEST_ASSERT_EQUAL_INT(2, sender_send_file("session", NULL, 9000, "input", 1, 1,
                                               0.0, 0.0, 0.0));
    TEST_ASSERT_EQUAL_INT(2, sender_send_file("session", "relay", 9000, NULL, 1, 1,
                                               0.0, 0.0, 0.0));
    TEST_ASSERT_EQUAL_INT(2, receiver_receive_file("session", NULL, 9000, "output"));
    TEST_ASSERT_EQUAL_INT(2, receiver_receive_file("session", "relay", 9000, NULL));
    TEST_ASSERT_FALSE(relay_client_open(NULL, "localhost", 9000));
    TEST_ASSERT_FALSE(relay_client_open(&client, NULL, 9000));
    relay_client_close(NULL);
    relay_client_close(&client);
    TEST_ASSERT_FALSE(relay_client_register_receiver(NULL, "session"));
    TEST_ASSERT_FALSE(relay_client_register_receiver(&client, "session"));
    client.socket_fd = 1;
    TEST_ASSERT_FALSE(relay_client_register_receiver(&client, "bad_name"));
    client.socket_fd = -1;
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.0, 0.0));
    client.socket_fd = 1;
    TEST_ASSERT_FALSE(relay_client_register_sender(NULL, "session", 0.0, 0.0, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", NAN, 0.0, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, NAN, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.0, NAN));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", -0.1, 0.0, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.6, 0.0, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, -0.1, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.6, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.0, -0.1));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.0, 0.6));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, INFINITY, 0.0));
    TEST_ASSERT_EQUAL_INT(-1,
                          relay_client_send_datagram(NULL, datagram, sizeof(datagram)));
    TEST_ASSERT_EQUAL_INT(RELAY_IO_ERROR,
                          relay_client_receive_packet(NULL, 0, &packet));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "bad_name", 0.0, 0.0, 0.0));
    TEST_ASSERT_EQUAL_INT(-1, relay_client_send_datagram(&client, NULL, 0));
    client.clock_failed = 1;
    TEST_ASSERT_FALSE(relay_client_clock_ok(&client));
    TEST_ASSERT_EQUAL_INT(RELAY_IO_ERROR,
                          relay_client_receive_packet(&client, 0, &packet));
    client.socket_fd = -1;
    client.clock_failed = 0;
    TEST_ASSERT_EQUAL_INT(-1, relay_client_send_datagram(&client, datagram, sizeof(datagram)));
    TEST_ASSERT_EQUAL_INT(RELAY_IO_ERROR,
                          relay_client_receive_packet(&client, 0, &packet));
    TEST_ASSERT_TRUE(relay_client_clock_ok(&client));
    (void)relay_client_now(&client);
    TEST_ASSERT_TRUE(relay_client_clock_ok(&client));
    TEST_ASSERT_EQUAL_UINT64(0, relay_client_now(NULL));
    TEST_ASSERT_FALSE(relay_client_clock_ok(NULL));
}

/* Verify remaining protocol edge cases and overflow guards are covered. */
void test_protocol_guard_and_overflow_paths(void)
{
    sender_state_t sender;
    sender_action_t action;
    unsigned char data = 'x';
    packet_t packet = {0};
    packet_t decoded = {0};
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length = 0;

    TEST_ASSERT_TRUE(sender_state_init(&sender, 1, 1));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&sender, &data, 1, UINT64_MAX, &action));
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, sender.timer_due_ms);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, action.timer_due_ms);

    packet.type = PACKET_FIN;
    packet.seq = 42U;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_EQUAL_INT(PACKET_FIN, datagram[0]);
    TEST_ASSERT_TRUE(packet_decode(datagram, datagram_length, &decoded));
    TEST_ASSERT_EQUAL_INT(PACKET_FIN, decoded.type);
    TEST_ASSERT_EQUAL_UINT64(42U, decoded.seq);
}

/* Verify a zero-timeout read still receives a packet that is already queued. */
void test_relay_client_zero_timeout_checks_queued_packet(void)
{
    int sockets[2] = {-1, -1};
    relay_client_t client = {-1, {0}, 0, 0};
    packet_t outgoing = {0};
    packet_t incoming = {0};
    unsigned char datagram[PACKET_HEADER_SIZE];
    size_t datagram_length = 0;
    int encode_ok;
    ssize_t sent = -1;
    relay_io_result_t packet_status = RELAY_IO_ERROR;
    relay_io_result_t empty_status = RELAY_IO_ERROR;

    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets));
    client.socket_fd = sockets[0];
    outgoing.type = PACKET_ACK;
    outgoing.seq = 3;
    encode_ok = packet_encode(&outgoing, datagram, sizeof(datagram), &datagram_length);
    if (encode_ok) {
        sent = send(sockets[1], datagram, datagram_length, 0);
        if (sent == (ssize_t)datagram_length) {
            packet_status = relay_client_receive_packet(&client, 0, &incoming);
            if (packet_status == RELAY_IO_PACKET) {
                empty_status = relay_client_receive_packet(&client, 0, &incoming);
            }
        }
    }
    (void)close(sockets[0]);
    (void)close(sockets[1]);
    client.socket_fd = -1;

    TEST_ASSERT_TRUE(encode_ok);
    TEST_ASSERT_EQUAL_INT(datagram_length, sent);
    TEST_ASSERT_EQUAL_INT(RELAY_IO_PACKET, packet_status);
    TEST_ASSERT_EQUAL_INT(PACKET_ACK, incoming.type);
    TEST_ASSERT_EQUAL_UINT64(3, incoming.seq);
    TEST_ASSERT_EQUAL_INT(RELAY_IO_TIMEOUT, empty_status);
}

/* Verify rejected/malformed registration replies and retry exhaustion are handled. */
void test_relay_registration_rejects_bad_replies_and_retries(void)
{
    TEST_ASSERT_TRUE(test_registration_result("ERR denied", 1, 0, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result("NO", 1, 0, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result("FAIL", 1, 0, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result(NULL, 5, 0, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result("OK_PACKET", 1, 1, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result("ERR denied", 1, 2, 1, 1));
    TEST_ASSERT_TRUE(test_registration_result("ERR denied", 1, 2, 2, 1));
    TEST_ASSERT_TRUE(test_registration_result("OK", 1, 2, 1, 0));
}

/* Verify network receiver timeout and sequence-overflow errors are reported. */
void test_receiver_network_timeout_and_state_error_paths(void)
{
    int timed_out;
    int state_error;

    receiver_io_test_set_mode(RECEIVER_IO_TEST_EXPIRED);
    timed_out = test_registration_result("OK", 1, 2, 2, 1);
    receiver_io_test_set_mode(RECEIVER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(timed_out);

    receiver_io_test_set_mode(RECEIVER_IO_TEST_SEQUENCE_OVERFLOW);
    state_error = test_registration_result("OK_DATA_MAX", 1, 2, 2, 1);
    receiver_io_test_set_mode(RECEIVER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(state_error);
}

/* Verify the network sender fails when the relay repeatedly drops its DATA packets. */
void test_sender_network_reports_repeated_timeout_failure(void)
{
    char input_path[] = "/tmp/cs525-timeout-XXXXXX";
    struct sockaddr_in relay_address = {0};
    socklen_t relay_address_length = sizeof(relay_address);
    int relay_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int input_fd;
    pid_t relay_pid;
    int relay_status;
    int sender_status;
    const unsigned char payload = 'x';

    TEST_ASSERT_TRUE(relay_fd >= 0);
    relay_address.sin_family = AF_INET;
    relay_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    TEST_ASSERT_EQUAL_INT(0, bind(relay_fd, (struct sockaddr *)&relay_address,
                                  sizeof(relay_address)));
    TEST_ASSERT_EQUAL_INT(0, getsockname(relay_fd, (struct sockaddr *)&relay_address,
                                         &relay_address_length));
    input_fd = mkstemp(input_path);
    TEST_ASSERT_TRUE(input_fd >= 0);
    TEST_ASSERT_EQUAL_INT(sizeof(payload), write(input_fd, &payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_INT(0, close(input_fd));

    relay_pid = fork();
    TEST_ASSERT_TRUE(relay_pid >= 0);
    if (relay_pid == 0) {
        int status;
        __gcov_reset();
        status = test_drop_ack_server(relay_fd);
        __gcov_dump();
        _exit(status);
    }
    TEST_ASSERT_EQUAL_INT(0, close(relay_fd));
    sender_status = sender_send_file("coveragetest", "127.0.0.1",
                                     ntohs(relay_address.sin_port), input_path,
                                     1, 10, 0.0, 0.0, 0.0);
    TEST_ASSERT_EQUAL_INT(relay_pid, waitpid(relay_pid, &relay_status, 0));
    TEST_ASSERT_EQUAL_INT(2, sender_status);
    TEST_ASSERT_TRUE(WIFEXITED(relay_status));
    TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(relay_status));
    TEST_ASSERT_EQUAL_INT(0, unlink(input_path));
}

/* Verify network sender guards reject a missing timer and malformed ACK payload. */
void test_sender_network_state_guards(void)
{
    int inactive_timer;
    int nonempty_ack;
    int data_limit;
    int fin_sequence_limit;

    TEST_ASSERT_TRUE(sender_io_test_rejects_invalid_action());
    TEST_ASSERT_TRUE(receiver_io_test_rejects_invalid_packet());
    sender_io_test_set_mode(SENDER_IO_TEST_DISARM_TIMER_AFTER_FIN);
    inactive_timer = test_registration_result("OK", 1, 2, 1, 1);
    sender_io_test_set_mode(SENDER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(inactive_timer);

    sender_io_test_set_mode(SENDER_IO_TEST_NONEMPTY_ACK);
    nonempty_ack = test_registration_result("OK_PACKET", 1, 2, 1, 1);
    sender_io_test_set_mode(SENDER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(nonempty_ack);

    sender_io_test_set_mode(SENDER_IO_TEST_DATA_LIMIT);
    data_limit = test_registration_result("OK", 1, 2, 1, 1);
    sender_io_test_set_mode(SENDER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(data_limit);

    sender_io_test_set_mode(SENDER_IO_TEST_FIN_SEQUENCE_LIMIT);
    fin_sequence_limit = test_registration_result("OK", 1, 2, 1, 1);
    sender_io_test_set_mode(SENDER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(fin_sequence_limit);
}

/* Register and execute the project's Unity unit and integration tests. */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_session_name_validation);
    RUN_TEST(test_checksum_matches_rfc1071_example);
    RUN_TEST(test_checksum_handles_odd_length_and_detects_a_flipped_bit);
    RUN_TEST(test_packet_wire_format_round_trips_and_checks_checksum);
    RUN_TEST(test_packet_decoder_rejects_invalid_header_and_size);
    RUN_TEST(test_packet_decoder_rejects_short_length_mismatch_and_unknown_type);
    RUN_TEST(test_packet_api_rejects_invalid_arguments);
    RUN_TEST(test_receiver_acknowledges_only_in_order_data);
    RUN_TEST(test_receiver_fin_acknowledges_and_lingers_until_deadline);
    RUN_TEST(test_receiver_idle_timeout_uses_passed_time);
    RUN_TEST(test_receiver_state_rejects_invalid_transitions_and_saturates);
    RUN_TEST(test_sender_window_full_cumulative_ack_and_duplicate_ack);
    RUN_TEST(test_sender_timeout_resends_every_packet_in_the_window);
    RUN_TEST(test_sender_retransmits_a_lost_packet_and_completes_in_memory);
    RUN_TEST(test_sender_stops_after_ten_expired_timers);
    RUN_TEST(test_sender_allows_eof_after_maximum_data_packet_count);
    RUN_TEST(test_sender_state_rejects_invalid_transitions_and_saturates);
    RUN_TEST(test_empty_file_transfers_without_data_packets);
    RUN_TEST(test_exact_multiple_of_packet_payload_transfers_without_empty_data);
    RUN_TEST(test_complete_transfer_survives_deterministic_bidirectional_impairment);
    RUN_TEST(test_network_api_rejects_invalid_arguments);
    RUN_TEST(test_protocol_guard_and_overflow_paths);
    RUN_TEST(test_relay_client_zero_timeout_checks_queued_packet);
    RUN_TEST(test_relay_registration_rejects_bad_replies_and_retries);
    RUN_TEST(test_receiver_network_timeout_and_state_error_paths);
    RUN_TEST(test_sender_network_reports_repeated_timeout_failure);
    RUN_TEST(test_sender_network_state_guards);
    RUN_TEST(test_network_transfer_through_loopback_relay);
    RUN_TEST(test_empty_network_transfer_sends_only_fin);
    RUN_TEST(test_exact_payload_network_transfer);
    return UNITY_END();
}