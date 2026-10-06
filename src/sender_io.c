#include "sender_io.h"

#include "receiver_gbn.h"
#include "relay_io.h"
#include "sender_gbn.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef TEST
enum {
    SENDER_IO_TEST_NORMAL = 0,
    SENDER_IO_TEST_DISARM_TIMER_AFTER_FIN = 1,
    SENDER_IO_TEST_NONEMPTY_ACK = 2,
    SENDER_IO_TEST_DATA_LIMIT = 3,
    SENDER_IO_TEST_FIN_SEQUENCE_LIMIT = 4
};

static int sender_io_test_mode;

/* Set a test-only condition that forces a sender network guard path. */
void sender_io_test_set_mode(int mode)
{
    sender_io_test_mode = mode;
}

const char *sender_io_test_input_path(void)
{
    return sender_io_test_mode == SENDER_IO_TEST_DATA_LIMIT ? "/dev/zero" : "/dev/null";
}

#endif

/* Encode and transmit every packet requested by the sender state machine. */
static int send_actions(relay_client_t *client, const sender_action_t *action)
{
    size_t index;

    for (index = 0; index < action->send_count; index++) {
        unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
        size_t datagram_length;

        if (!packet_encode(&action->packets[index], datagram, sizeof(datagram),
                           &datagram_length)) {
            return 0;
        }
        /* Excluded: socket send errors depend on external OS/network conditions. */
        if (relay_client_send_datagram(client, datagram, datagram_length) != 0) { /* GCOVR_EXCL_START */
            return 0;
            /* GCOVR_EXCL_STOP */
        }
    }
    return 1;
}

#ifdef TEST
int sender_io_test_rejects_invalid_action(void)
{
    relay_client_t client = {-1, {0}, 0, 0};
    sender_action_t action = {0};

    action.send_count = 1;
    action.packets[0].type = (packet_type_t)3;
    return !send_actions(&client, &action);
}
#endif

/* Read input, drive the sender state machine, and exchange packets with the relay. */
static int transfer_file(FILE *input,
                         relay_client_t *client,
                         uint32_t window_size,
                         uint64_t timeout_ms)
{
    sender_state_t state;

    if (!sender_state_init(&state, window_size, timeout_ms)) {
        fprintf(stderr, "Invalid sender window or timeout.\n");
        return 2;
    }
#ifdef TEST
    if (sender_io_test_mode == SENDER_IO_TEST_DATA_LIMIT) {
        state.base = SENDER_MAX_DATA_PACKETS;
        state.next = SENDER_MAX_DATA_PACKETS;
    } else if (sender_io_test_mode == SENDER_IO_TEST_FIN_SEQUENCE_LIMIT) {
        state.base = UINT32_MAX;
        state.next = UINT32_MAX;
    }
#endif

    for (;;) {
        while (sender_can_accept_data(&state)) {
            unsigned char payload[PACKET_MAX_PAYLOAD];
            size_t payload_length = fread(payload, 1, sizeof(payload), input);
            uint64_t now_ms;

            /* Excluded: input read failures depend on filesystem/device errors. */
            if (ferror(input)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not read input file.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
            now_ms = relay_client_now(client);
            /* Excluded: monotonic clock failure is a host operating-system error. */
            if (!relay_client_clock_ok(client)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not read monotonic clock.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
            if (payload_length > 0) {
                sender_action_t action;
                if (sender_on_data(&state, payload, payload_length, now_ms, &action) ==
                    SENDER_ERROR) {
                    fprintf(stderr, "Could not send DATA packet.\n");
                    return 2;
                }
                /* Excluded: packet-send failures are external socket/network errors. */
                if (!send_actions(client, &action)) { /* GCOVR_EXCL_START */
                    fprintf(stderr, "Could not send DATA packet.\n");
                    return 2;
                    /* GCOVR_EXCL_STOP */
                }
            }
            if (feof(input)) {
                sender_action_t action;
                if (sender_on_eof(&state, now_ms, &action) == SENDER_ERROR) {
                    fprintf(stderr, "Could not send FIN packet.\n");
                    return 2;
                }
                /* Excluded: packet-send failures are external socket/network errors. */
                if (!send_actions(client, &action)) { /* GCOVR_EXCL_START */
                    fprintf(stderr, "Could not send FIN packet.\n");
                    return 2;
                    /* GCOVR_EXCL_STOP */
                }
#ifdef TEST
                if (sender_io_test_mode == SENDER_IO_TEST_DISARM_TIMER_AFTER_FIN) {
                    state.timer_armed = 0;
                }
#endif
                break;
            }
            /* A zero-byte read without EOF or error is a stdio no-progress condition. */
            if (payload_length == 0) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Input file read made no progress.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
        }

        if (state.completed) {
            return 0;
        }
        if (!state.timer_armed) {
            fprintf(stderr, "Sender has outstanding packets without an active timer.\n");
            return 2;
        }

        {
            packet_t incoming;
            uint64_t now_ms = relay_client_now(client);
            uint64_t wait_ms;
            relay_io_result_t io_status;
            sender_action_t action;
            sender_status_t state_status;

            /* Excluded: this defensive branch requires a host clock failure. */
            if (!relay_client_clock_ok(client)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not read monotonic clock.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
            wait_ms = state.timer_due_ms > now_ms ? state.timer_due_ms - now_ms : 0;
            io_status = relay_client_receive_packet(client, wait_ms, &incoming);
            /* Excluded: receive errors depend on external socket/OS conditions. */
            if (io_status == RELAY_IO_ERROR) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Network error while waiting for ACK.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
            if (io_status == RELAY_IO_INVALID) {
                continue;
            }
            now_ms = relay_client_now(client);
            /* Excluded: this defensive branch requires a host clock failure. */
            if (!relay_client_clock_ok(client)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not read monotonic clock.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }

            if (io_status == RELAY_IO_PACKET) {
#ifdef TEST
                if (sender_io_test_mode == SENDER_IO_TEST_NONEMPTY_ACK &&
                    incoming.type == PACKET_ACK) {
                    incoming.payload_length = 1;
                }
#endif
                if (incoming.type != PACKET_ACK || incoming.payload_length != 0) {
                    continue;
                }
                state_status = sender_on_ack(&state, incoming.seq, now_ms, &action);
            } else {
                state_status = sender_on_timeout(&state, now_ms, &action);
            }
            if (state_status == SENDER_ERROR) {
                fprintf(stderr, "Sender failed after repeated timeouts or invalid state.\n");
                return 2;
            }
            /* Excluded: packet-send failures are external socket/network errors. */
            if (!send_actions(client, &action)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not send protocol packet.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
        }
    }
}

/* Register as sender, open the input file, and run the retransmitting transfer. */
int sender_send_file(const char *session,
                     const char *relay,
                     uint16_t port,
                     const char *file_path,
                     uint32_t window_size,
                     uint64_t timeout_ms,
                     double loss,
                     double corrupt,
                     double duplicate)
{
    relay_client_t client = {-1, {0}, 0, 0};
    FILE *input;
    int status = 2;

    if (!receiver_session_valid(session)) {
        fprintf(stderr, "Invalid session name.\n");
        return 1;
    }
    if (relay == NULL || file_path == NULL) {
        return 2;
    }
    /* Excluded: DNS and UDP socket setup failures depend on host networking. */
    if (!relay_client_open(&client, relay, port)) { /* GCOVR_EXCL_START */
        return 2;
        /* GCOVR_EXCL_STOP */
    }
    if (!relay_client_register_sender(&client, session, loss, corrupt, duplicate)) {
        goto cleanup;
    }

    input = fopen(file_path, "rb");
    /* Excluded: file-open errors depend on external filesystem permissions/state. */
    if (input == NULL) { /* GCOVR_EXCL_START */
        fprintf(stderr, "Cannot open input file '%s': %s\n", file_path, strerror(errno));
        goto cleanup;
        /* GCOVR_EXCL_STOP */
    }
    status = transfer_file(input, &client, window_size, timeout_ms);
    {
        int close_status = fclose(input);
        if (status == 0) {
            /* Excluded: close failure depends on external filesystem/device state. */
            if (close_status != 0) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not close input file cleanly.\n");
                status = 2;
                /* GCOVR_EXCL_STOP */
            }
        }
    }

cleanup:
    relay_client_close(&client);
    return status;
}