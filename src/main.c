#define _POSIX_C_SOURCE 200809L

#include "receiver.h"
#include "receiver_net.h"
#include "sender.h"
#include "sender_net.h"
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <unistd.h>

#ifdef TEST
#define main main_exclude
#endif

/* Holds parsed command values, including defaults for optional settings. */
typedef struct {
    const char *session;
    const char *relay;
    const char *file;
    long window;
    long timeout_ms;
    long port;
    double loss;
    double corrupt;
    double dup;
} options_t;

/* Print the command syntax and describe every supported argument. */
static void print_usage(FILE *stream)
{
    fprintf(stream,
        "Usage: myapp send -s <session> [-w window] [-T timeout-ms] [-l loss]\n"
        "                  [-c corrupt] [-d dup] [-p port] <relay> <file>\n"
        "       myapp recv -s <session> [-p port] <relay> <file>\n"
        "\n"
        "  -s <session>     session name shared by the sender and the receiver\n"
        "  -w <window>      Go-Back-N window size in packets, 1 to 64 (default: 8)\n"
        "  -T <timeout-ms>  retransmission timeout in milliseconds (default: 250)\n"
        "  -l <loss>        probability the relay drops a packet (default: 0)\n"
        "  -c <corrupt>     probability the relay flips a bit (default: 0)\n"
        "  -d <dup>         probability the relay duplicates a packet (default: 0)\n"
        "  -p <port>        relay port (default: 4250)\n"
        "  <relay>          host name or address of the relay\n"
        "  <file>           file to send, or file to write what is received\n");
}

/* Convert a complete decimal integer and enforce the caller's allowed range. */
static int parse_long(const char *text, long minimum, long maximum, long *value)
{
    char *end;
    long parsed;

    errno = 0;
    parsed = strtol(text, &end, 10);
    if (errno != 0 || end == text || *end != '\0' || parsed < minimum || parsed > maximum) {
        return 0;
    }
    *value = parsed;
    return 1;
}

/* Parse a relay probability, which must be a number from 0 through 1. */
static int parse_probability(const char *text, double *value)
{
    char *end;
    double parsed;

    errno = 0;
    parsed = strtod(text, &end);
    if (errno != 0 || end == text || *end != '\0' || !isfinite(parsed) ||
        parsed < 0.0 || parsed > 0.5) {
        return 0;
    }
    *value = parsed;
    return 1;
}

int main(int argc, char *argv[])
{
    /* Defaults apply unless the user supplies the corresponding send option. */
    options_t options = {NULL, NULL, NULL, 8, 250, 4250, 0.0, 0.0, 0.0};
    int is_send;
    int option;
    int positional_count;

    /* No-argument execution is the clean usage path used by `make leak`. */
    if (argc == 1) {
        print_usage(stdout);
        return 0;
    }

    /* The mode is always the first argument; getopt handles all remaining flags. */
    if (strcmp(argv[1], "send") != 0 && strcmp(argv[1], "recv") != 0) {
        print_usage(stderr);
        return 1;
    }
    is_send = strcmp(argv[1], "send") == 0;

    /* Start option parsing after the mode instead of treating it as a flag. */
    optind = 2;
    opterr = 0;
    while ((option = getopt(argc, argv, "s:w:T:l:c:d:p:")) != -1) {
        switch (option) {
        case 's':
            options.session = optarg;
            break;
        case 'p':
            if (!parse_long(optarg, 1, 65535, &options.port)) {
                fprintf(stderr, "Invalid relay port: %s\n", optarg);
                return 1;
            }
            break;
        case 'w':
            if (!is_send || !parse_long(optarg, 1, 64, &options.window)) {
                fprintf(stderr, "Invalid or unsupported window option: %s\n", optarg);
                return 1;
            }
            break;
        case 'T':
            if (!is_send || !parse_long(optarg, 1, LONG_MAX, &options.timeout_ms)) {
                fprintf(stderr, "Invalid or unsupported timeout option: %s\n", optarg);
                return 1;
            }
            break;
        case 'l':
            if (!is_send || !parse_probability(optarg, &options.loss)) {
                fprintf(stderr, "Invalid or unsupported loss probability: %s\n", optarg);
                return 1;
            }
            break;
        case 'c':
            if (!is_send || !parse_probability(optarg, &options.corrupt)) {
                fprintf(stderr, "Invalid or unsupported corruption probability: %s\n", optarg);
                return 1;
            }
            break;
        case 'd':
            if (!is_send || !parse_probability(optarg, &options.dup)) {
                fprintf(stderr, "Invalid or unsupported duplication probability: %s\n", optarg);
                return 1;
            }
            break;
        default:
            fprintf(stderr, "Invalid command-line option.\n");
            print_usage(stderr);
            return 1;
        }
    }

    /* Require a nonempty session and exactly the relay and file operands. */
    positional_count = argc - optind;
    if (options.session == NULL || options.session[0] == '\0' || positional_count != 2) {
        fprintf(stderr, "A session, relay, and file are required.\n");
        print_usage(stderr);
        return 1;
    }
    if (!receiver_session_valid(options.session)) {
        fprintf(stderr, "Invalid session name. Use 1 to 32 lowercase letters, digits, or hyphens.\n");
        return 1;
    }
    options.relay = argv[optind];
    options.file = argv[optind + 1];
    /* Reject empty operands before a later networking or file operation uses them. */
    if (options.relay[0] == '\0' || options.file[0] == '\0') {
        fprintf(stderr, "Relay and file must not be empty.\n");
        return 1;
    }

    if (!is_send) {
        return receiver_receive_file(options.session, options.relay,
                                     (uint16_t)options.port, options.file);
    }

    return sender_send_file(options.session, options.relay, (uint16_t)options.port,
                            options.file, (uint32_t)options.window,
                            (uint64_t)options.timeout_ms, options.loss,
                            options.corrupt, options.dup);
}