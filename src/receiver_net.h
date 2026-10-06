#ifndef RECEIVER_NET_H
#define RECEIVER_NET_H

#include <stdint.h>

/* Receive one session's file through the relay and store it at file_path. */
int receiver_receive_file(const char *session,
                          const char *relay,
                          uint16_t port,
                          const char *file_path);

#endif