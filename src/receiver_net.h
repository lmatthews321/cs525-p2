#ifndef RECEIVER_NET_H
#define RECEIVER_NET_H

#include <stdint.h>

int receiver_receive_file(const char *session,
                          const char *relay,
                          uint16_t port,
                          const char *file_path);

#endif