#include "sender_net.h"

#include "receiver.h"
#include "relay_client.h"
#include "sender.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int send_actions(relay_client_t *client, const sender_action_t *action)
{
    size_t index;

    for (index = 0; index < action->send_count; index++) {
        unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
        size_t datagram_length;

        if (!packet_encode(&action->packets[index], datagram, sizeof(datagram),
                           &datagram_length) ||
            relay_client_send_datagram(client, datagram, datagram_length) != 0) {
            return 0;
        }
    }
    return 1;
}

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

    for (;;) {
        while (sender_can_accept_data(&state)) {
            unsigned char payload[PACKET_MAX_PAYLOAD];
            size_t payload_length = fread(payload, 1, sizeof(payload), input);
            uint64_t now_ms;

            if (ferror(input)) {
                fprintf(stderr, "Could not read input file.\n");
                return 2;
            }
            now_ms = relay_client_now(client);
            if (!relay_client_clock_ok(client)) {
                fprintf(stderr, "Could not read monotonic clock.\n");
                return 2;
            }
            if (payload_length > 0) {
                sender_action_t action;
                if (sender_on_data(&state, payload, payload_length, now_ms, &action) ==
                        SENDER_ERROR || !send_actions(client, &action)) {
                    fprintf(stderr, "Could not send DATA packet.\n");
                    return 2;
                }
            }
            if (feof(input)) {
                sender_action_t action;
                if (sender_on_eof(&state, now_ms, &action) == SENDER_ERROR ||
                    !send_actions(client, &action)) {
                    fprintf(stderr, "Could not send FIN packet.\n");
                    return 2;
                }
                break;
            }
            if (payload_length == 0) {
                fprintf(stderr, "Input file read made no progress.\n");
                return 2;
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

            if (!relay_client_clock_ok(client)) {
                fprintf(stderr, "Could not read monotonic clock.\n");
                return 2;
            }
            wait_ms = state.timer_due_ms > now_ms ? state.timer_due_ms - now_ms : 0;
            io_status = relay_client_receive_packet(client, wait_ms, &incoming);
            if (io_status == RELAY_IO_ERROR) {
                fprintf(stderr, "Network error while waiting for ACK.\n");
                return 2;
            }
            if (io_status == RELAY_IO_INVALID) {
                continue;
            }
            now_ms = relay_client_now(client);
            if (!relay_client_clock_ok(client)) {
                fprintf(stderr, "Could not read monotonic clock.\n");
                return 2;
            }

            if (io_status == RELAY_IO_PACKET) {
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
            if (!send_actions(client, &action)) {
                fprintf(stderr, "Could not send protocol packet.\n");
                return 2;
            }
        }
    }
}

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
    if (relay == NULL || file_path == NULL || !relay_client_open(&client, relay, port)) {
        return 2;
    }
    if (!relay_client_register_sender(&client, session, loss, corrupt, duplicate)) {
        goto cleanup;
    }

    input = fopen(file_path, "rb");
    if (input == NULL) {
        fprintf(stderr, "Cannot open input file '%s': %s\n", file_path, strerror(errno));
        goto cleanup;
    }
    status = transfer_file(input, &client, window_size, timeout_ms);
    if (fclose(input) != 0 && status == 0) {
        fprintf(stderr, "Could not close input file cleanly.\n");
        status = 2;
    }

cleanup:
    relay_client_close(&client);
    return status;
}