#ifndef SENDER_NET_H
#define SENDER_NET_H

#include <stdint.h>

int sender_send_file(const char *session,
                     const char *relay,
                     uint16_t port,
                     const char *file_path,
                     uint32_t window_size,
                     uint64_t timeout_ms,
                     double loss,
                     double corrupt,
                     double duplicate);

#endif