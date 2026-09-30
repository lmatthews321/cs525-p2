#include <math.h>
#include <stdio.h>
#include <string.h>

#include "harness/unity.h"
#include "../src/packet.h"
#include "../src/receiver.h"
#include "../src/receiver_net.h"
#include "../src/relay_client.h"
#include "../src/sender.h"
#include "../src/sender_net.h"

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

static uint32_t test_random_next(test_channel_t *channel)
{
    uint64_t value = channel->random_state;

    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    channel->random_state = value;
    return (uint32_t)((value * UINT64_C(2685821657736338717)) >> 32);
}

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

void setUp(void) {}
void tearDown(void) {}

void test_checksum_matches_rfc1071_example(void)
{
    const unsigned char example[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    TEST_ASSERT_EQUAL_INT(0x220d, packet_checksum(example, sizeof(example)));
}

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

void test_session_name_validation(void)
{
    TEST_ASSERT_TRUE(receiver_session_valid("jdoe-1"));
    TEST_ASSERT_TRUE(receiver_session_valid("abcdefghijklmnopqrstuvwxyz123456"));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid(""));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid("Bad_Name"));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid("abcdefghijklmnopqrstuvwxyz1234567"));
}

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
    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_timeout(&state, 2499, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_DONE,
                          receiver_on_timeout(&state, 2500, &action));
    TEST_ASSERT_FALSE(action.timer_armed);
}

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

void test_network_api_rejects_invalid_arguments(void)
{
    relay_client_t client = {-1, {0}, 0, 0};
    unsigned char datagram[PACKET_HEADER_SIZE] = {0};
    packet_t packet;

    TEST_ASSERT_EQUAL_INT(1, sender_send_file(NULL, "relay", 9000, "input", 1, 1,
                                               0.0, 0.0, 0.0));
    TEST_ASSERT_EQUAL_INT(1, receiver_receive_file(NULL, "relay", 9000, "output"));
    TEST_ASSERT_FALSE(relay_client_open(NULL, "localhost", 9000));
    TEST_ASSERT_FALSE(relay_client_open(&client, NULL, 9000));
    relay_client_close(NULL);
    relay_client_close(&client);
    TEST_ASSERT_FALSE(relay_client_register_receiver(NULL, "session"));
    TEST_ASSERT_FALSE(relay_client_register_receiver(&client, "session"));
    client.socket_fd = 1;
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", NAN, 0.0, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.6, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "bad_name", 0.0, 0.0, 0.0));
    client.socket_fd = -1;
    TEST_ASSERT_EQUAL_INT(-1, relay_client_send_datagram(&client, datagram, sizeof(datagram)));
    TEST_ASSERT_EQUAL_INT(RELAY_IO_ERROR,
                          relay_client_receive_packet(&client, 0, &packet));
    TEST_ASSERT_TRUE(relay_client_clock_ok(&client));
    (void)relay_client_now(&client);
    TEST_ASSERT_TRUE(relay_client_clock_ok(&client));
}

int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_session_name_validation);
    RUN_TEST(test_checksum_matches_rfc1071_example);
    RUN_TEST(test_checksum_handles_odd_length_and_detects_a_flipped_bit);
    RUN_TEST(test_packet_wire_format_round_trips_and_checks_checksum);
    RUN_TEST(test_packet_decoder_rejects_invalid_header_and_size);
    RUN_TEST(test_packet_decoder_rejects_short_length_mismatch_and_unknown_type);
    RUN_TEST(test_receiver_acknowledges_only_in_order_data);
    RUN_TEST(test_receiver_fin_acknowledges_and_lingers_until_deadline);
    RUN_TEST(test_receiver_idle_timeout_uses_passed_time);
    RUN_TEST(test_sender_window_full_cumulative_ack_and_duplicate_ack);
    RUN_TEST(test_sender_timeout_resends_every_packet_in_the_window);
    RUN_TEST(test_sender_retransmits_a_lost_packet_and_completes_in_memory);
    RUN_TEST(test_sender_stops_after_ten_expired_timers);
    RUN_TEST(test_sender_allows_eof_after_maximum_data_packet_count);
    RUN_TEST(test_empty_file_transfers_without_data_packets);
    RUN_TEST(test_exact_multiple_of_packet_payload_transfers_without_empty_data);
    RUN_TEST(test_complete_transfer_survives_deterministic_bidirectional_impairment);
    RUN_TEST(test_network_api_rejects_invalid_arguments);
    return UNITY_END();
}