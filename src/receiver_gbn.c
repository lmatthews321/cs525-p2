#include "receiver_gbn.h"

#include <string.h>

/* Add durations without wrapping deadlines near the uint64_t limit. */
static uint64_t add_saturated(uint64_t value, uint64_t amount)
{
    return value > UINT64_MAX - amount ? UINT64_MAX : value + amount;
}

/* Clear an action and attach the timer state implied by the receiver status. */
static void set_action(receiver_state_t *state,
                       receiver_status_t status,
                       receiver_action_t *action)
{
    memset(action, 0, sizeof(*action));
    action->status = status;
    if (status == RECEIVER_CONTINUE || status == RECEIVER_LINGERING) {
        action->timer_armed = 1;
        action->timer_due_ms = receiver_next_deadline(state);
    }
}

/* Accept only nonempty session names of up to 32 lowercase letters, digits, or hyphens. */
int receiver_session_valid(const char *session)
{
    size_t length = 0;

    if (session == NULL) {
        return 0;
    }
    while (session[length] != '\0') {
        unsigned char character = (unsigned char)session[length];
        if (length >= 32 ||
            !((character >= 'a' && character <= 'z') ||
              (character >= '0' && character <= '9') || character == '-')) {
            return 0;
        }
        length++;
    }
    return length >= 1;
}

/* Start a receiver at sequence zero and record when its idle timeout begins. */
void receiver_state_init(receiver_state_t *state, uint64_t now_ms)
{
    if (state == NULL) {
        return;
    }
    memset(state, 0, sizeof(*state));
    state->last_valid_ms = now_ms;
}

/* Return the active linger or idle deadline for the receiver state. */
uint64_t receiver_next_deadline(const receiver_state_t *state)
{
    if (state == NULL) {
        return 0;
    }
    return state->lingering
        ? state->linger_deadline_ms
        : add_saturated(state->last_valid_ms, RECEIVER_IDLE_TIMEOUT_MS);
}

/* Process DATA, ACK, or FIN and describe payload delivery and ACK work to perform. */
receiver_status_t receiver_on_packet(receiver_state_t *state,
                                     const packet_t *packet,
                                     uint64_t now_ms,
                                     receiver_action_t *action)
{
    receiver_status_t status;

    if (state == NULL || packet == NULL || action == NULL ||
        (packet->type != PACKET_DATA && packet->type != PACKET_ACK &&
         packet->type != PACKET_FIN) || packet->payload_length > PACKET_MAX_PAYLOAD ||
        (packet->type != PACKET_DATA && packet->payload_length != 0)) {
        return RECEIVER_ERROR;
    }

    status = state->lingering ? RECEIVER_LINGERING : RECEIVER_CONTINUE;
    set_action(state, status, action);
    if (state->lingering) {
        if (packet->type == PACKET_FIN && packet->seq == state->fin_sequence) {
            action->send_ack = 1;
            action->ack_sequence = state->expected;
        }
        return status;
    }
    if (packet->type == PACKET_ACK) {
        return status;
    }

    state->last_valid_ms = now_ms;
    if (packet->type == PACKET_DATA) {
        if (packet->seq == state->expected) {
            if (state->expected == UINT32_MAX) {
                return RECEIVER_ERROR;
            }
            action->deliver_payload = packet->payload_length > 0;
            action->payload_length = packet->payload_length;
            if (packet->payload_length > 0) {
                memcpy(action->payload, packet->payload, packet->payload_length);
            }
            state->expected++;
        }
        action->send_ack = 1;
        action->ack_sequence = state->expected;
        action->timer_due_ms = receiver_next_deadline(state);
        return RECEIVER_CONTINUE;
    }

    if (packet->seq != state->expected) {
        action->send_ack = 1;
        action->ack_sequence = state->expected;
        action->timer_due_ms = receiver_next_deadline(state);
        return RECEIVER_CONTINUE;
    }
    if (state->expected == UINT32_MAX) {
        return RECEIVER_ERROR;
    }

    state->fin_sequence = packet->seq;
    state->expected++;
    state->lingering = 1;
    state->linger_deadline_ms = add_saturated(now_ms, RECEIVER_LINGER_MS);
    action->status = RECEIVER_LINGERING;
    action->send_ack = 1;
    action->ack_sequence = state->expected;
    action->timer_due_ms = state->linger_deadline_ms;
    return RECEIVER_LINGERING;
}

/* Advance the receiver when its timer fires, reporting idle timeout or completion. */
receiver_status_t receiver_on_timeout(receiver_state_t *state,
                                      uint64_t now_ms,
                                      receiver_action_t *action)
{
    receiver_status_t status;

    if (state == NULL || action == NULL) {
        return RECEIVER_ERROR;
    }
    if (state->lingering) {
        status = now_ms >= state->linger_deadline_ms ? RECEIVER_DONE : RECEIVER_LINGERING;
    } else if (now_ms >= state->last_valid_ms &&
               now_ms - state->last_valid_ms >= RECEIVER_IDLE_TIMEOUT_MS) {
        status = RECEIVER_TIMED_OUT;
    } else {
        status = RECEIVER_CONTINUE;
    }
    set_action(state, status, action);
    return status;
}