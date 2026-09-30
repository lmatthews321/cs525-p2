#include "receiver_net.h"

#include "receiver.h"
#include "relay_client.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

static int send_packet(relay_client_t *client, const packet_t *packet)
{
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length;

    return packet_encode(packet, datagram, sizeof(datagram), &datagram_length) &&
           relay_client_send_datagram(client, datagram, datagram_length) == 0;
}

int receiver_receive_file(const char *session,
                          const char *relay,
                          uint16_t port,
                          const char *file_path)
{
    relay_client_t client = {-1, {0}, 0, 0};
    receiver_state_t state;
    FILE *output = NULL;
    int status = 2;

    if (!receiver_session_valid(session)) {
        fprintf(stderr, "Invalid session name.\n");
        return 1;
    }
    if (relay == NULL || file_path == NULL || !relay_client_open(&client, relay, port)) {
        return 2;
    }
    if (!relay_client_register_receiver(&client, session)) {
        goto cleanup;
    }

    output = fopen(file_path, "wb");
    if (output == NULL) {
        fprintf(stderr, "Cannot open received file '%s': %s\n", file_path, strerror(errno));
        goto cleanup;
    }
    receiver_state_init(&state, relay_client_now(&client));
    if (!relay_client_clock_ok(&client)) {
        fprintf(stderr, "Could not read monotonic clock.\n");
        goto cleanup;
    }

    for (;;) {
        packet_t packet;
        receiver_action_t action;
        uint64_t now_ms = relay_client_now(&client);
        uint64_t deadline;
        uint64_t wait_ms;
        receiver_status_t state_status;
        relay_io_result_t io_status;

        if (!relay_client_clock_ok(&client)) {
            fprintf(stderr, "Could not read monotonic clock.\n");
            break;
        }
        state_status = receiver_on_timeout(&state, now_ms, &action);
        if (state_status == RECEIVER_DONE) {
            status = 0;
            break;
        }
        if (state_status == RECEIVER_TIMED_OUT || state_status == RECEIVER_ERROR) {
            fprintf(stderr, "Receiver timed out or the transfer failed.\n");
            break;
        }

        deadline = action.timer_due_ms;
        wait_ms = deadline > now_ms ? deadline - now_ms : 0;
        io_status = relay_client_receive_packet(&client, wait_ms, &packet);
        if (io_status == RELAY_IO_ERROR) {
            fprintf(stderr, "Network error while waiting for DATA.\n");
            break;
        }
        if (io_status != RELAY_IO_PACKET) {
            continue;
        }

        now_ms = relay_client_now(&client);
        if (!relay_client_clock_ok(&client)) {
            fprintf(stderr, "Could not read monotonic clock.\n");
            break;
        }
        state_status = receiver_on_packet(&state, &packet, now_ms, &action);
        if (state_status == RECEIVER_ERROR) {
            fprintf(stderr, "Invalid receiver state transition.\n");
            break;
        }
        if (action.deliver_payload &&
            fwrite(action.payload, 1, action.payload_length, output) != action.payload_length) {
            fprintf(stderr, "Could not write received file.\n");
            break;
        }
        if (action.send_ack) {
            packet_t ack = {0};
            ack.type = PACKET_ACK;
            ack.seq = action.ack_sequence;
            if (!send_packet(&client, &ack)) {
                fprintf(stderr, "Could not send ACK packet.\n");
                break;
            }
        }
    }

cleanup:
    if (output != NULL && fclose(output) != 0 && status == 0) {
        fprintf(stderr, "Could not close received file cleanly.\n");
        status = 2;
    }
    relay_client_close(&client);
    return status;
}