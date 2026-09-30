#ifndef SENDER_H
#define SENDER_H

#include "packet.h"

#include <stdint.h>

#define SENDER_MAX_WINDOW 64U
#define SENDER_MAX_DATA_PACKETS UINT32_C(16384)
#define SENDER_MAX_TIMEOUTS 10U

typedef struct {
    uint32_t base;
    uint32_t next;
    uint32_t window_size;
    uint32_t consecutive_timeouts;
    uint64_t timeout_ms;
    uint64_t timer_due_ms;
    packet_t outstanding[SENDER_MAX_WINDOW];
    int timer_armed;
    int eof;
    int fin_sent;
    int completed;
    int failed;
} sender_state_t;

typedef enum {
    SENDER_ERROR = -1,
    SENDER_ACTIVE = 0,
    SENDER_DONE = 1
} sender_status_t;

typedef struct {
    sender_status_t status;
    size_t send_count;
    packet_t packets[SENDER_MAX_WINDOW];
    int timer_armed;
    uint64_t timer_due_ms;
} sender_action_t;

int sender_state_init(sender_state_t *state,
                      uint32_t window_size,
                      uint64_t timeout_ms);
int sender_can_accept_data(const sender_state_t *state);
sender_status_t sender_on_data(sender_state_t *state,
                               const unsigned char *payload,
                               size_t payload_length,
                               uint64_t now_ms,
                               sender_action_t *action);
sender_status_t sender_on_eof(sender_state_t *state,
                              uint64_t now_ms,
                              sender_action_t *action);
sender_status_t sender_on_ack(sender_state_t *state,
                              uint32_t next_expected,
                              uint64_t now_ms,
                              sender_action_t *action);
sender_status_t sender_on_timeout(sender_state_t *state,
                                  uint64_t now_ms,
                                  sender_action_t *action);

#endif