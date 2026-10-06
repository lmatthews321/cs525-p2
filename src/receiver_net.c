#include "receiver_net.h"

#include "receiver.h"
#include "relay_client.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef TEST
enum {
    RECEIVER_NET_TEST_NORMAL = 0,
    RECEIVER_NET_TEST_EXPIRED = 1,
    RECEIVER_NET_TEST_SEQUENCE_OVERFLOW = 2
};

static int receiver_net_test_mode;

/* Set a test-only condition that forces a receiver network error path. */
void receiver_net_test_set_mode(int mode)
{
    receiver_net_test_mode = mode;
}
#endif

/* Encode and send one protocol packet through the connected relay client. */
static int send_packet(relay_client_t *client, const packet_t *packet)
{
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length;

    /* Excluded: the receiver state machine only constructs valid ACK packets. */
    if (!packet_encode(packet, datagram, sizeof(datagram), &datagram_length)) { /* GCOVR_EXCL_START */
        return 0;
        /* GCOVR_EXCL_STOP */
    }
    /* Excluded branch: UDP send failure is controlled by OS/socket conditions. */
    return relay_client_send_datagram(client, datagram, datagram_length) == 0; /* GCOVR_EXCL_BR_LINE */
}

/* Register as receiver, write in-order data to disk, ACK packets, and await FIN. */
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
    if (relay == NULL || file_path == NULL) {
        return 2;
    }
    /* Excluded: resolver/socket setup failures depend on host networking. */
    if (!relay_client_open(&client, relay, port)) { /* GCOVR_EXCL_START */
        return 2;
        /* GCOVR_EXCL_STOP */
    }
    if (!relay_client_register_receiver(&client, session)) {
        goto cleanup;
    }

    output = fopen(file_path, "wb");
    /* Excluded: file-open failure depends on external filesystem permissions/state. */
    if (output == NULL) { /* GCOVR_EXCL_START */
        fprintf(stderr, "Cannot open received file '%s': %s\n", file_path, strerror(errno));
        goto cleanup;
        /* GCOVR_EXCL_STOP */
    }
    receiver_state_init(&state, relay_client_now(&client));
    /* Excluded: monotonic clock failure is a host operating-system error. */
    if (!relay_client_clock_ok(&client)) { /* GCOVR_EXCL_START */
        fprintf(stderr, "Could not read monotonic clock.\n");
        goto cleanup;
        /* GCOVR_EXCL_STOP */
    }
#ifdef TEST
    if (receiver_net_test_mode == RECEIVER_NET_TEST_SEQUENCE_OVERFLOW) {
        state.expected = UINT32_MAX;
    }
#endif

    for (;;) {
        packet_t packet;
        receiver_action_t action;
        uint64_t now_ms = relay_client_now(&client);
        uint64_t deadline;
        uint64_t wait_ms;
        receiver_status_t state_status;
        relay_io_result_t io_status;

        /* Excluded: monotonic clock failure is not reliably inducible in tests. */
        if (!relay_client_clock_ok(&client)) { /* GCOVR_EXCL_START */
            fprintf(stderr, "Could not read monotonic clock.\n");
            break;
            /* GCOVR_EXCL_STOP */
        }
#ifdef TEST
        if (receiver_net_test_mode == RECEIVER_NET_TEST_EXPIRED) {
            now_ms = state.last_valid_ms + RECEIVER_IDLE_TIMEOUT_MS;
        }
#endif
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
        /* Excluded: poll/recv system-call errors depend on external socket state. */
        if (io_status == RELAY_IO_ERROR) { /* GCOVR_EXCL_START */
            fprintf(stderr, "Network error while waiting for DATA.\n");
            break;
            /* GCOVR_EXCL_STOP */
        }
        if (io_status != RELAY_IO_PACKET) {
            continue;
        }

        now_ms = relay_client_now(&client);
        /* Excluded: this is another defensive path for host clock failure. */
        if (!relay_client_clock_ok(&client)) { /* GCOVR_EXCL_START */
            fprintf(stderr, "Could not read monotonic clock.\n");
            break;
            /* GCOVR_EXCL_STOP */
        }
        state_status = receiver_on_packet(&state, &packet, now_ms, &action);
        if (state_status == RECEIVER_ERROR) {
            fprintf(stderr, "Invalid receiver state transition.\n");
            break;
        }
        if (action.deliver_payload) {
            /* Excluded: short writes depend on filesystem/device errors. */
            if (fwrite(action.payload, 1, action.payload_length, output) !=
                action.payload_length) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not write received file.\n");
                break;
                /* GCOVR_EXCL_STOP */
            }
        }
        if (action.send_ack) {
            packet_t ack = {0};
            ack.type = PACKET_ACK;
            ack.seq = action.ack_sequence;
            /* Excluded: packet-send failure requires an OS/network error. */
            if (!send_packet(&client, &ack)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not send ACK packet.\n");
                break;
                /* GCOVR_EXCL_STOP */
            }
        }
    }

cleanup:
    if (output != NULL) {
        int close_status = fclose(output);
        if (status == 0) {
            /* Excluded: close failure depends on external filesystem/device state. */
            if (close_status != 0) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not close received file cleanly.\n");
                status = 2;
                /* GCOVR_EXCL_STOP */
            }
        }
    }
    relay_client_close(&client);
    return status;
}