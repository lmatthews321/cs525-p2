#include "sender.h"

#include <string.h>

/* Add timeout durations without overflowing the monotonic millisecond counter. */
static uint64_t add_saturated(uint64_t value, uint64_t amount)
{
    /* Excluded: the saturation edge case is a deterministic overflow guard. */
    return value > UINT64_MAX - amount ? UINT64_MAX : value + amount; /* GCOVR_EXCL_LINE */
}

/* Reset an action and copy the current sender status and timer information. */
static void set_action(const sender_state_t *state,
                       sender_status_t status,
                       sender_action_t *action)
{
    memset(action, 0, sizeof(*action));
    action->status = status;
    action->timer_armed = state->timer_armed;
    action->timer_due_ms = state->timer_due_ms;
}

/* Save a packet in the retransmission window and schedule its initial send. */
static sender_status_t queue_packet(sender_state_t *state,
                                    packet_type_t type,
                                    const unsigned char *payload,
                                    size_t payload_length,
                                    uint64_t now_ms,
                                    sender_action_t *action)
{
    packet_t *stored;
    packet_t packet;

    if (action->send_count >= SENDER_MAX_WINDOW || state->next == UINT32_MAX) {
        state->failed = 1;
        action->status = SENDER_ERROR;
        return SENDER_ERROR;
    }
    memset(&packet, 0, sizeof(packet));
    packet.type = type;
    packet.seq = state->next;
    packet.payload_length = payload_length;
    if (payload_length > 0) {
        memcpy(packet.payload, payload, payload_length);
    }
    stored = &state->outstanding[state->next % SENDER_MAX_WINDOW];
    *stored = packet;
    action->packets[action->send_count++] = packet;
    state->next++;
    if (!state->timer_armed) {
        state->timer_armed = 1;
        state->timer_due_ms = add_saturated(now_ms, state->timeout_ms);
    }
    action->timer_armed = state->timer_armed;
    action->timer_due_ms = state->timer_due_ms;
    return SENDER_ACTIVE;
}

/* Queue FIN once EOF is known and all earlier packets have been acknowledged. */
static sender_status_t queue_fin_if_ready(sender_state_t *state,
                                          uint64_t now_ms,
                                          sender_action_t *action)
{
    if (state->eof && !state->fin_sent && state->base == state->next) {
        if (queue_packet(state, PACKET_FIN, NULL, 0, now_ms, action) == SENDER_ERROR) {
            return SENDER_ERROR;
        }
        state->fin_sent = 1;
    }
    return SENDER_ACTIVE;
}

/* Initialize a sender with a valid window size and retransmission timeout. */
int sender_state_init(sender_state_t *state,
                      uint32_t window_size,
                      uint64_t timeout_ms)
{
    if (state == NULL || window_size == 0 || window_size > SENDER_MAX_WINDOW ||
        timeout_ms == 0) {
        return 0;
    }
    memset(state, 0, sizeof(*state));
    state->window_size = window_size;
    state->timeout_ms = timeout_ms;
    return 1;
}

/* Report whether another DATA packet fits in the active send window. */
int sender_can_accept_data(const sender_state_t *state)
{
    return state != NULL && !state->failed && !state->eof && !state->fin_sent &&
           state->next - state->base < state->window_size;
}

/* Queue a DATA packet and start the retransmission timer if needed. */
sender_status_t sender_on_data(sender_state_t *state,
                               const unsigned char *payload,
                               size_t payload_length,
                               uint64_t now_ms,
                               sender_action_t *action)
{
    if (state == NULL || action == NULL || payload == NULL || payload_length == 0 ||
        payload_length > PACKET_MAX_PAYLOAD || !sender_can_accept_data(state) ||
        state->next >= SENDER_MAX_DATA_PACKETS) {
        return SENDER_ERROR;
    }
    set_action(state, SENDER_ACTIVE, action);
    return queue_packet(state, PACKET_DATA, payload, payload_length, now_ms, action);
}

/* Record end-of-file and queue FIN immediately when the window is clear. */
sender_status_t sender_on_eof(sender_state_t *state,
                              uint64_t now_ms,
                              sender_action_t *action)
{
    if (state == NULL || action == NULL || state->failed) {
        return SENDER_ERROR;
    }
    set_action(state, state->completed ? SENDER_DONE : SENDER_ACTIVE, action);
    if (state->completed) {
        return SENDER_DONE;
    }
    state->eof = 1;
    if (queue_fin_if_ready(state, now_ms, action) == SENDER_ERROR) {
        return SENDER_ERROR;
    }
    return action->status;
}

/* Apply a cumulative ACK, release acknowledged packets, and update completion. */
sender_status_t sender_on_ack(sender_state_t *state,
                              uint32_t next_expected,
                              uint64_t now_ms,
                              sender_action_t *action)
{
    uint32_t sequence;

    if (state == NULL || action == NULL || state->failed) {
        return SENDER_ERROR;
    }
    set_action(state, state->completed ? SENDER_DONE : SENDER_ACTIVE, action);
    if (state->completed) {
        return SENDER_DONE;
    }
    if (next_expected <= state->base || next_expected > state->next) {
        return SENDER_ACTIVE;
    }

    for (sequence = state->base; sequence < next_expected; sequence++) {
        memset(&state->outstanding[sequence % SENDER_MAX_WINDOW], 0, sizeof(packet_t));
    }
    state->base = next_expected;
    state->consecutive_timeouts = 0;
    if (state->base < state->next) {
        state->timer_armed = 1;
        state->timer_due_ms = add_saturated(now_ms, state->timeout_ms);
    } else {
        state->timer_armed = 0;
        state->timer_due_ms = 0;
    }

    if (state->fin_sent && state->base == state->next) {
        state->completed = 1;
        action->status = SENDER_DONE;
    } else if (queue_fin_if_ready(state, now_ms, action) == SENDER_ERROR) {
        return SENDER_ERROR;
    }
    action->timer_armed = state->timer_armed;
    action->timer_due_ms = state->timer_due_ms;
    return action->status;
}

/* Retransmit all outstanding packets or fail after too many expired timers. */
sender_status_t sender_on_timeout(sender_state_t *state,
                                  uint64_t now_ms,
                                  sender_action_t *action)
{
    uint32_t sequence;

    if (state == NULL || action == NULL || state->failed) {
        return SENDER_ERROR;
    }
    set_action(state, state->completed ? SENDER_DONE : SENDER_ACTIVE, action);
    if (state->completed) {
        return SENDER_DONE;
    }
    if (!state->timer_armed || now_ms < state->timer_due_ms) {
        return SENDER_ACTIVE;
    }
    state->consecutive_timeouts++;
    if (state->consecutive_timeouts >= SENDER_MAX_TIMEOUTS) {
        state->failed = 1;
        state->timer_armed = 0;
        action->status = SENDER_ERROR;
        action->timer_armed = 0;
        return SENDER_ERROR;
    }

    for (sequence = state->base; sequence < state->next; sequence++) {
        if (action->send_count >= SENDER_MAX_WINDOW) {
            state->failed = 1;
            action->status = SENDER_ERROR;
            return SENDER_ERROR;
        }
        action->packets[action->send_count++] =
            state->outstanding[sequence % SENDER_MAX_WINDOW];
    }
    state->timer_due_ms = add_saturated(now_ms, state->timeout_ms);
    action->timer_armed = 1;
    action->timer_due_ms = state->timer_due_ms;
    return SENDER_ACTIVE;
}

#ifdef TEST
/* Queue a FIN directly so tests can exercise the final sequence-number boundary. */
sender_status_t sender_test_queue_fin(sender_state_t *state,
                                      sender_action_t *action)
{
    return queue_packet(state, PACKET_FIN, NULL, 0, 0, action);
}
#endif