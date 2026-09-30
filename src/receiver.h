#ifndef RECEIVER_H
#define RECEIVER_H

#include "packet.h"

#include <stdint.h>

#define RECEIVER_IDLE_TIMEOUT_MS UINT64_C(30000)
#define RECEIVER_LINGER_MS UINT64_C(2000)

typedef struct {
    uint32_t expected;
    uint32_t fin_sequence;
    uint64_t last_valid_ms;
    uint64_t linger_deadline_ms;
    int lingering;
} receiver_state_t;

typedef enum {
    RECEIVER_ERROR = -1,
    RECEIVER_CONTINUE = 0,
    RECEIVER_LINGERING = 1,
    RECEIVER_TIMED_OUT = 2,
    RECEIVER_DONE = 3
} receiver_status_t;

typedef struct {
    receiver_status_t status;
    int send_ack;
    uint32_t ack_sequence;
    int deliver_payload;
    size_t payload_length;
    unsigned char payload[PACKET_MAX_PAYLOAD];
    int timer_armed;
    uint64_t timer_due_ms;
} receiver_action_t;

int receiver_session_valid(const char *session);
void receiver_state_init(receiver_state_t *state, uint64_t now_ms);
receiver_status_t receiver_on_packet(receiver_state_t *state,
                                     const packet_t *packet,
                                     uint64_t now_ms,
                                     receiver_action_t *action);
receiver_status_t receiver_on_timeout(receiver_state_t *state,
                                      uint64_t now_ms,
                                      receiver_action_t *action);
uint64_t receiver_next_deadline(const receiver_state_t *state);

#endif