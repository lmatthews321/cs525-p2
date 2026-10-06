# Submission Report

- Submission generated at 10/06/2026 at 15:41:38

- Machine info: Linux runnervmmprz5 6.17.0-1022-azure #22-Ubuntu SMP Mon Jul 27 17:24:03 UTC 2026 x86_64 x86_64 x86_64 GNU/Linux

## Note to Students

Please read this report carefully before submission.
Ensure that all sections are complete and accurate.
Look for any errors in the build or test outputs.
If you find any issues, correct them before submitting.
Post any questions on the class discussion board for help.


---

## README

# Project cs525-p2 Reliable Data Transfer

- Name: Lael Matthews
- Email: laelmatthews@u.boisestate.edu
- Class: CS525

## Known Bugs or Issues

There is a possibility that the FIN recovery can fail if the timeout is excessively long. However with a receiver linger time of 2 seconds this is unlikely. 
There are 47 coverage-exclusion annotations: 39 excluded blocks, 7 branch exclusions, and 1 line exclusion. They cover outcomes controlled by external system and library calls, including DNS, sockets, polling, clocks, and file/stdio operations.

## Experience

This was the first project that utilized the AI agent exclusively for all code production. It was magical how quickly a project of this magnitude could be generated. However, with all the generated code it's difficult to really understand everything that was generated. I can get the feel for each function, but fully understanding everything would take almost as long and trying to write all the code myself. It's like black box programing now. We say what inputs and outputs we want and just leave the internals to AI. I enjoyed testing out the finished project by sending and receiving through the relay and understand the concepts of the Go-Back-N protocol from this lab. 

## Analysis

The primary analysis for this project is how the window size affects the performance of our Go-Back-N protocol. A larger window reduces time and increases throughput because more packets can be sent at a time. Loss does affect larger window systems more. They are still faster, but suffer a higher percent of degradation as discussed in the results below. Another feature of the protocol that is tested is the checksum which validates that messages arrive uncorrupted. The program successfully computed the expected value in the lab test file and during actual data transfer through the relay. The three other main pieces of the protocol (sequence numbers, cumulative acknowledgements, and the sender timer) all help ensure packets are received in the correct order. This is proven to work by noting that the file received matches the one that was sent, even if loss, corruption and/or duplication are added. I attempted a few different combinations of these variables and all received files matching the original sent.

## Design
This project separates packet handling, protocol logic, and I/O so the Go-Back-N state machines can be tested without real sockets, clocks, or files.

The packet layer computes checksums, encodes packets into byte buffers, and decodes and validates buffers into packets. It consists of `packet.c` and `packet.h`.

The Go-Back-N state-machine layer implements the sender and receiver as state structs with status and action results. Its functions process protocol events and, where needed, take the current time in milliseconds; they return actions such as packets to send, payload to deliver, or the next timer deadline. Sender events include data becoming available, input reaching EOF, a cumulative ACK arriving, and retransmission timeout. Receiver events include DATA, FIN, or ACK packet arrival and timer expiration. The receiver processes DATA and FIN; ACK packets are ignored. Errors are returned as statuses rather than delivered as events. This layer consists of `sender_gbn.c`, `sender_gbn.h`, `receiver_gbn.c`, and `receiver_gbn.h`.

The I/O layer handles file operations, relay registration, sockets, polling, and the real clock. It translates I/O results into state-machine events and carries out the actions returned by the state machines. Sender- and receiver-specific I/O is in `sender_io.c` / `sender_io.h` and `receiver_io.c` / `receiver_io.h`. Shared relay I/O is in `relay_io.c` and `relay_io.h`.



## Results

| Window | Loss | Corrupt | Dup | Mean Time (s) | Throughput (KiB/s) |
| ---: | ---: | ---: | ---: | ---: | ---: |
| 1 | 0 | 0 | 0 | 103.59 | 9.89 |
| 16 | 0 | 0 | 0 | 6.60 | 155.15 |
| 1 | 0.05 | 0 | 0 | 131.45 | 7.79 |
| 16 | 0.05 | 0 | 0 | 21.72 | 47.15 |

#### From the window 1, no loss run, compute the round trip time your sender actually saw. (It sent 1025 packets and waited one round trip for each.) The relay adds 100 ms. Where does the rest come from?

From the first run we can say the relay round trip of 100ms times 1025 packets is 102.5 seconds (or 101.1ms per packet). The remaining 1.09 seconds(1.1ms per packet) is composed of overhead, transmission time, and processing time. The transmission time is the time it takes to push a packet out of the socket. Processing time is the time it takes to process the packet. Other overhead may include socket calls, and OS scheduling.

#### Use that round trip time to explain the speedup at window 16. Is it close to 16 times? Why or why not?

The total time for the window = 16 case was 6.6 seconds. This is about 15.7 times the window = 1 case. It should be pretty close to 16 because 16 packets can be sent before the sender is required to wait for the ACK packets to arrive. Since the round trip time accounts for the bulk of the delay, all 16 acks will be received in quick succession allowing another 16 packets to be sent. Essentially this will look like a burst of 16 packets about every 101ms. The total ratio isn't exactly 16 because of the overheads mentioned in the previous question. 

#### Why does 5% loss cost the window 16 run more than it costs the window 1 run? Think about what a timeout makes the sender resend in each case.
A 5% loss costs more for the window 16 run because it will have to resend all the packets from the lost packet onward after the timeout is reached. This limits the data it transfers from the bursts of 16 to some shorter burst (depending on what packet was lost). Compared to the window 1 case this is a significant change to throughput. The window 1 case only has to retransmit a single packet. 
---


## Build Output

This section was generated by running `make all` in the project root directory.

```bash
make[1]: Entering directory '/home/runner/work/cs525-p2/cs525-p2'
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/packet.c -o build/debug/packet.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/relay_io.c -o build/debug/relay_io.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/receiver_gbn.c -o build/debug/receiver_gbn.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/sender_gbn.c -o build/debug/sender_gbn.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/receiver_io.c -o build/debug/receiver_io.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/sender_io.c -o build/debug/sender_io.c.o
mkdir -p build/debug
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address -c src/main.c -o build/debug/main.c.o
cc -g -O0 -DDEBUG -fno-omit-frame-pointer -fsanitize=address build/debug/packet.c.o build/debug/relay_io.c.o build/debug/receiver_gbn.c.o build/debug/sender_gbn.c.o build/debug/receiver_io.c.o build/debug/sender_io.c.o build/debug/main.c.o -o build/debug/myapp_d -fsanitize=address
make[1]: Leaving directory '/home/runner/work/cs525-p2/cs525-p2'
make[1]: Entering directory '/home/runner/work/cs525-p2/cs525-p2'
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/packet.c -o build/release/packet.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/relay_io.c -o build/release/relay_io.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/receiver_gbn.c -o build/release/receiver_gbn.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/sender_gbn.c -o build/release/sender_gbn.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/receiver_io.c -o build/release/receiver_io.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/sender_io.c -o build/release/sender_io.c.o
mkdir -p build/release
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion -c src/main.c -o build/release/main.c.o
cc -Wall -Wextra -O2 -fPIE -MMD -MP -Wformat -Wformat=2 -Wconversion -Wsign-conversion -Wimplicit-fallthrough -fstack-protector-strong -Werror=format-security -Werror=implicit -Werror=incompatible-pointer-types -Werror=int-conversion build/release/packet.c.o build/release/relay_io.c.o build/release/receiver_gbn.c.o build/release/sender_gbn.c.o build/release/receiver_io.c.o build/release/sender_io.c.o build/release/main.c.o -o build/release/myapp 
make[1]: Leaving directory '/home/runner/work/cs525-p2/cs525-p2'
make[1]: Entering directory '/home/runner/work/cs525-p2/cs525-p2'
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/packet.c -o build/tests/packet.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/relay_io.c -o build/tests/relay_io.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/receiver_gbn.c -o build/tests/receiver_gbn.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/sender_gbn.c -o build/tests/sender_gbn.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/receiver_io.c -o build/tests/receiver_io.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/sender_io.c -o build/tests/sender_io.c.o
mkdir -p build/tests
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c src/main.c -o build/tests/main.c.o
mkdir -p build/tests/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/lab-test.c -o build/tests/lab-test.c.o
mkdir -p build/tests/harness/
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage -c tests/harness/unity.c -o build/tests/harness/unity.c.o
cc -g -O0 -DTEST -fprofile-arcs -ftest-coverage build/tests/packet.c.o build/tests/relay_io.c.o build/tests/receiver_gbn.c.o build/tests/sender_gbn.c.o build/tests/receiver_io.c.o build/tests/sender_io.c.o build/tests/main.c.o build/tests/lab-test.c.o build/tests/harness/unity.c.o -o build/tests/myapp_t -fprofile-arcs -ftest-coverage
make[1]: Leaving directory '/home/runner/work/cs525-p2/cs525-p2'
make[1]: Entering directory '/home/runner/work/cs525-p2/cs525-p2'
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage -c src/packet.c -o build/debug-test/packet.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage -c src/relay_io.c -o build/debug-test/relay_io.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage -c src/receiver_gbn.c -o build/debug-test/receiver_gbn.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage -c src/sender_gbn.c -o build/debug-test/sender_gbn.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage -c src/receiver_io.c -o build/debug-test/receiver_io.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage -c src/sender_io.c -o build/debug-test/sender_io.c.o
mkdir -p build/debug-test
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage -c src/main.c -o build/debug-test/main.c.o
mkdir -p build/debug-test/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage -c tests/lab-test.c -o build/debug-test/lab-test.c.o
mkdir -p build/debug-test/harness/
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage -c tests/harness/unity.c -o build/debug-test/harness/unity.c.o
cc -g -O0 -DDEBUG -DTEST -fno-omit-frame-pointer -fsanitize=address -fprofile-arcs -ftest-coverage build/debug-test/packet.c.o build/debug-test/relay_io.c.o build/debug-test/receiver_gbn.c.o build/debug-test/sender_gbn.c.o build/debug-test/receiver_io.c.o build/debug-test/sender_io.c.o build/debug-test/main.c.o build/debug-test/lab-test.c.o build/debug-test/harness/unity.c.o -o build/debug-test/myapp_td -fsanitize=address -fprofile-arcs -ftest-coverage
make[1]: Leaving directory '/home/runner/work/cs525-p2/cs525-p2'
Builds completed. You can run the application with: ./build/release/myapp
You can run the debug build with: ./build/debug/myapp_d
You can run the test build with: ./build/tests/myapp_t
You can run the debug-test build with: ./build/debug-test/myapp_td
```

---

## Coverage Report

This section was generated by running `make report` in the project root directory.

```bash
Invalid session name.
Invalid session name.
Relay refused registration: denied
Relay returned an invalid registration reply.
Relay returned an invalid registration reply.
Relay did not answer registration after five attempts.
Relay refused registration: denied
Relay refused registration: denied
Invalid sender window or timeout.
Receiver timed out or the transfer failed.
Invalid receiver state transition.
Sender failed after repeated timeouts or invalid state.
Sender has outstanding packets without an active timer.
Network error while waiting for ACK.
Could not send DATA packet.
Could not send FIN packet.
tests/lab-test.c:1612:test_session_name_validation:PASS
tests/lab-test.c:1613:test_checksum_matches_rfc1071_example:PASS
tests/lab-test.c:1614:test_checksum_handles_odd_length_and_detects_a_flipped_bit:PASS
tests/lab-test.c:1615:test_packet_wire_format_round_trips_and_checks_checksum:PASS
tests/lab-test.c:1616:test_packet_decoder_rejects_invalid_header_and_size:PASS
tests/lab-test.c:1617:test_packet_decoder_rejects_short_length_mismatch_and_unknown_type:PASS
tests/lab-test.c:1618:test_packet_api_rejects_invalid_arguments:PASS
tests/lab-test.c:1619:test_receiver_acknowledges_only_in_order_data:PASS
tests/lab-test.c:1620:test_receiver_fin_acknowledges_and_lingers_until_deadline:PASS
tests/lab-test.c:1621:test_receiver_idle_timeout_uses_passed_time:PASS
tests/lab-test.c:1622:test_receiver_state_rejects_invalid_transitions_and_saturates:PASS
tests/lab-test.c:1623:test_sender_window_full_cumulative_ack_and_duplicate_ack:PASS
tests/lab-test.c:1624:test_sender_timeout_resends_every_packet_in_the_window:PASS
tests/lab-test.c:1625:test_sender_retransmits_a_lost_packet_and_completes_in_memory:PASS
tests/lab-test.c:1626:test_sender_stops_after_ten_expired_timers:PASS
tests/lab-test.c:1627:test_sender_allows_eof_after_maximum_data_packet_count:PASS
tests/lab-test.c:1628:test_sender_state_rejects_invalid_transitions_and_saturates:PASS
tests/lab-test.c:1629:test_empty_file_transfers_without_data_packets:PASS
tests/lab-test.c:1630:test_exact_multiple_of_packet_payload_transfers_without_empty_data:PASS
tests/lab-test.c:1631:test_complete_transfer_survives_deterministic_bidirectional_impairment:PASS
tests/lab-test.c:1632:test_network_api_rejects_invalid_arguments:PASS
tests/lab-test.c:1633:test_protocol_guard_and_overflow_paths:PASS
tests/lab-test.c:1634:test_relay_client_zero_timeout_checks_queued_packet:PASS
tests/lab-test.c:1635:test_relay_registration_rejects_bad_replies_and_retries:PASS
tests/lab-test.c:1636:test_receiver_network_timeout_and_state_error_paths:PASS
tests/lab-test.c:1637:test_sender_network_reports_repeated_timeout_failure:PASS
tests/lab-test.c:1638:test_sender_network_state_guards:PASS
tests/lab-test.c:1639:test_network_transfer_through_loopback_relay:PASS
tests/lab-test.c:1640:test_empty_network_transfer_sends_only_fin:PASS
tests/lab-test.c:1641:test_exact_payload_network_transfer:PASS

-----------------------
30 Tests 0 Failures 0 Ignored 
OK
mkdir -p ./build/report/html
mkdir -p ./build/report/txt
gcovr -r . build/tests --html --html-details \
	--exclude-directories tests/harness \
	--exclude-directories build/tests/harness \
	--exclude '.*main\.c$' --exclude '.*test\.c$' \
	--exclude '.*harness/.*' --exclude '.*unity\.c$' \
	-o ./build/report/html/coverage_report.html
(INFO) Reading coverage data...

(INFO) Writing coverage report...

gcovr -r . build/tests --txt \
	--exclude-directories tests/harness \
	--exclude-directories build/tests/harness \
	--exclude '.*main\.c$' --exclude '.*test\.c$' \
	--exclude '.*harness/.*' --exclude '.*unity\.c$' \
	| tee ./build/report/txt/coverage_report.txt
(INFO) Reading coverage data...

(INFO) Writing coverage report...

------------------------------------------------------------------------------
                           GCC Code Coverage Report
Directory: .
------------------------------------------------------------------------------
File                                       Lines     Exec  Cover   Missing
------------------------------------------------------------------------------
src/packet.c                                  58       58   100%
src/receiver_gbn.c                            87       87   100%
src/receiver_io.c                             60       60   100%
src/relay_io.c                               102      102   100%
src/sender_gbn.c                             115      115   100%
src/sender_io.c                               79       79   100%
------------------------------------------------------------------------------
TOTAL                                        501      501   100%
------------------------------------------------------------------------------
```

---

## Address Sanitizer Report

This section was generated by running `make leak-test` in the project root directory.

```bash
Invalid session name.
Invalid session name.
Relay refused registration: denied
Relay returned an invalid registration reply.
Relay returned an invalid registration reply.
Relay did not answer registration after five attempts.
Relay refused registration: denied
Relay refused registration: denied
Invalid sender window or timeout.
Receiver timed out or the transfer failed.
Invalid receiver state transition.
Sender failed after repeated timeouts or invalid state.
Sender has outstanding packets without an active timer.
Network error while waiting for ACK.
Could not send DATA packet.
Could not send FIN packet.
tests/lab-test.c:1612:test_session_name_validation:PASS
tests/lab-test.c:1613:test_checksum_matches_rfc1071_example:PASS
tests/lab-test.c:1614:test_checksum_handles_odd_length_and_detects_a_flipped_bit:PASS
tests/lab-test.c:1615:test_packet_wire_format_round_trips_and_checks_checksum:PASS
tests/lab-test.c:1616:test_packet_decoder_rejects_invalid_header_and_size:PASS
tests/lab-test.c:1617:test_packet_decoder_rejects_short_length_mismatch_and_unknown_type:PASS
tests/lab-test.c:1618:test_packet_api_rejects_invalid_arguments:PASS
tests/lab-test.c:1619:test_receiver_acknowledges_only_in_order_data:PASS
tests/lab-test.c:1620:test_receiver_fin_acknowledges_and_lingers_until_deadline:PASS
tests/lab-test.c:1621:test_receiver_idle_timeout_uses_passed_time:PASS
tests/lab-test.c:1622:test_receiver_state_rejects_invalid_transitions_and_saturates:PASS
tests/lab-test.c:1623:test_sender_window_full_cumulative_ack_and_duplicate_ack:PASS
tests/lab-test.c:1624:test_sender_timeout_resends_every_packet_in_the_window:PASS
tests/lab-test.c:1625:test_sender_retransmits_a_lost_packet_and_completes_in_memory:PASS
tests/lab-test.c:1626:test_sender_stops_after_ten_expired_timers:PASS
tests/lab-test.c:1627:test_sender_allows_eof_after_maximum_data_packet_count:PASS
tests/lab-test.c:1628:test_sender_state_rejects_invalid_transitions_and_saturates:PASS
tests/lab-test.c:1629:test_empty_file_transfers_without_data_packets:PASS
tests/lab-test.c:1630:test_exact_multiple_of_packet_payload_transfers_without_empty_data:PASS
tests/lab-test.c:1631:test_complete_transfer_survives_deterministic_bidirectional_impairment:PASS
tests/lab-test.c:1632:test_network_api_rejects_invalid_arguments:PASS
tests/lab-test.c:1633:test_protocol_guard_and_overflow_paths:PASS
tests/lab-test.c:1634:test_relay_client_zero_timeout_checks_queued_packet:PASS
tests/lab-test.c:1635:test_relay_registration_rejects_bad_replies_and_retries:PASS
tests/lab-test.c:1636:test_receiver_network_timeout_and_state_error_paths:PASS
tests/lab-test.c:1637:test_sender_network_reports_repeated_timeout_failure:PASS
tests/lab-test.c:1638:test_sender_network_state_guards:PASS
tests/lab-test.c:1639:test_network_transfer_through_loopback_relay:PASS
tests/lab-test.c:1640:test_empty_network_transfer_sends_only_fin:PASS
tests/lab-test.c:1641:test_exact_payload_network_transfer:PASS

-----------------------
30 Tests 0 Failures 0 Ignored 
OK
```

---

## Src Files
### main.c

```c

#define _POSIX_C_SOURCE 200809L

#include "receiver_gbn.h"
#include "receiver_io.h"
#include "sender_gbn.h"
#include "sender_io.h"
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
```

### packet.c

```c

#include "packet.h"

#include <arpa/inet.h>
#include <string.h>

enum {
    PACKET_TYPE_OFFSET = 0,
    PACKET_RESERVED_OFFSET = 1,
    PACKET_CHECKSUM_OFFSET = 2,
    PACKET_SEQUENCE_OFFSET = 4,
    PACKET_LENGTH_OFFSET = 8,
    PACKET_PAYLOAD_OFFSET = 10
};

/* Compute the Internet checksum over a byte buffer, including an odd final byte. */
uint16_t packet_checksum(const unsigned char *bytes, size_t length)
{
    uint32_t sum = 0;
    size_t index = 0;

    if (bytes == NULL && length != 0) {
        return 0;
    }
    while (index + 1 < length) {
        sum += ((uint32_t)bytes[index] << 8) | bytes[index + 1];
        index += 2;
    }
    if (index < length) {
        sum += (uint32_t)bytes[index] << 8;
    }
    while ((sum >> 16) != 0) {
        sum = (sum & UINT32_C(0xffff)) + (sum >> 16);
    }
    return (uint16_t)~sum;
}

/* Reject packet values that cannot be represented by the wire protocol. */
static int packet_valid(const packet_t *packet)
{
    if (packet == NULL || packet->payload_length > PACKET_MAX_PAYLOAD ||
        (packet->type != PACKET_DATA && packet->type != PACKET_ACK &&
         packet->type != PACKET_FIN)) {
        return 0;
    }
    return packet->type == PACKET_DATA || packet->payload_length == 0;
}

/* Serialize a validated packet and add its checksum in network byte order. */
int packet_encode(const packet_t *packet,
                  unsigned char *datagram,
                  size_t capacity,
                  size_t *datagram_length)
{
    uint16_t network_length;
    uint16_t network_checksum;
    uint32_t network_sequence;
    size_t packet_length;

    if (!packet_valid(packet) || datagram == NULL || datagram_length == NULL) {
        return 0;
    }
    packet_length = PACKET_HEADER_SIZE + packet->payload_length;
    if (capacity < packet_length) {
        return 0;
    }

    datagram[PACKET_TYPE_OFFSET] = (unsigned char)packet->type;
    datagram[PACKET_RESERVED_OFFSET] = 0;
    datagram[PACKET_CHECKSUM_OFFSET] = 0;
    datagram[PACKET_CHECKSUM_OFFSET + 1] = 0;
    network_sequence = htonl(packet->seq);
    memcpy(datagram + PACKET_SEQUENCE_OFFSET, &network_sequence, sizeof(network_sequence));
    network_length = htons((uint16_t)packet->payload_length);
    memcpy(datagram + PACKET_LENGTH_OFFSET, &network_length, sizeof(network_length));
    if (packet->payload_length > 0) {
        memcpy(datagram + PACKET_PAYLOAD_OFFSET, packet->payload, packet->payload_length);
    }

    network_checksum = htons(packet_checksum(datagram, packet_length));
    memcpy(datagram + PACKET_CHECKSUM_OFFSET, &network_checksum, sizeof(network_checksum));
    *datagram_length = packet_length;
    return 1;
}

/* Validate a wire datagram before decoding its header and payload into a packet. */
int packet_decode(const unsigned char *datagram,
                  size_t datagram_length,
                  packet_t *packet)
{
    uint16_t network_length;
    uint32_t network_sequence;
    size_t payload_length;

    if (datagram == NULL || packet == NULL || datagram_length < PACKET_HEADER_SIZE ||
        datagram[PACKET_RESERVED_OFFSET] != 0 ||
        datagram[PACKET_TYPE_OFFSET] > PACKET_FIN) {
        return 0;
    }
    memcpy(&network_length, datagram + PACKET_LENGTH_OFFSET, sizeof(network_length));
    payload_length = ntohs(network_length);
    if (payload_length > PACKET_MAX_PAYLOAD ||
        PACKET_HEADER_SIZE + payload_length != datagram_length ||
        (datagram[PACKET_TYPE_OFFSET] != PACKET_DATA && payload_length != 0) ||
        packet_checksum(datagram, datagram_length) != 0) {
        return 0;
    }

    memcpy(&network_sequence, datagram + PACKET_SEQUENCE_OFFSET, sizeof(network_sequence));
    packet->type = (packet_type_t)datagram[PACKET_TYPE_OFFSET];
    packet->seq = ntohl(network_sequence);
    packet->payload_length = payload_length;
    if (payload_length > 0) {
        memcpy(packet->payload, datagram + PACKET_PAYLOAD_OFFSET, payload_length);
    }
    return 1;
}
```

### packet.h

```c

#ifndef PACKET_H
#define PACKET_H

#include <stddef.h>
#include <stdint.h>

#define PACKET_HEADER_SIZE 10U
#define PACKET_MAX_PAYLOAD 1024U
#define PACKET_MAX_DATAGRAM_SIZE (PACKET_HEADER_SIZE + PACKET_MAX_PAYLOAD)

typedef enum {
    PACKET_DATA = 0,
    PACKET_ACK = 1,
    PACKET_FIN = 2
} packet_type_t;

typedef struct {
    packet_type_t type;
    uint32_t seq;
    size_t payload_length;
    unsigned char payload[PACKET_MAX_PAYLOAD];
} packet_t;

/* Return the Internet checksum for the supplied bytes. */
uint16_t packet_checksum(const unsigned char *bytes, size_t length);
/* Encode a packet when the output buffer has enough capacity. */
int packet_encode(const packet_t *packet,
                  unsigned char *datagram,
                  size_t capacity,
                  size_t *datagram_length);
/* Decode only datagrams with a valid header, length, type, and checksum. */
int packet_decode(const unsigned char *datagram,
                  size_t datagram_length,
                  packet_t *packet);

#endif
```

### receiver_gbn.c

```c

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
```

### receiver_gbn.h

```c

#ifndef RECEIVER_GBN_H
#define RECEIVER_GBN_H

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

/* Return whether session meets the protocol's length and character restrictions. */
int receiver_session_valid(const char *session);
/* Initialize receiver state and set its initial idle-time reference. */
void receiver_state_init(receiver_state_t *state, uint64_t now_ms);
/* Process an incoming packet and return the response and payload actions needed. */
receiver_status_t receiver_on_packet(receiver_state_t *state,
                                     const packet_t *packet,
                                     uint64_t now_ms,
                                     receiver_action_t *action);
/* Handle an idle or linger timer event. */
receiver_status_t receiver_on_timeout(receiver_state_t *state,
                                      uint64_t now_ms,
                                      receiver_action_t *action);
/* Get the next deadline at which the receiver should be woken. */
uint64_t receiver_next_deadline(const receiver_state_t *state);

#endif
```

### receiver_io.c

```c

#include "receiver_io.h"

#include "receiver_gbn.h"
#include "relay_io.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef TEST
enum {
    RECEIVER_IO_TEST_NORMAL = 0,
    RECEIVER_IO_TEST_EXPIRED = 1,
    RECEIVER_IO_TEST_SEQUENCE_OVERFLOW = 2
};

static int receiver_io_test_mode;

/* Set a test-only condition that forces a receiver network error path. */
void receiver_io_test_set_mode(int mode)
{
    receiver_io_test_mode = mode;
}

#endif

/* Encode and send one protocol packet through the connected relay client. */
static int send_packet(relay_client_t *client, const packet_t *packet)
{
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length;

    if (!packet_encode(packet, datagram, sizeof(datagram), &datagram_length)) {
        return 0;
    }
    /* Excluded branch: UDP send failure is controlled by OS/socket conditions. */
    return relay_client_send_datagram(client, datagram, datagram_length) == 0; /* GCOVR_EXCL_BR_LINE */
}

#ifdef TEST
int receiver_io_test_rejects_invalid_packet(void)
{
    relay_client_t client = {-1, {0}, 0, 0};
    packet_t packet = {0};

    packet.type = (packet_type_t)3;
    return !send_packet(&client, &packet);
}
#endif

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
    if (receiver_io_test_mode == RECEIVER_IO_TEST_SEQUENCE_OVERFLOW) {
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
        if (receiver_io_test_mode == RECEIVER_IO_TEST_EXPIRED) {
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
```

### receiver_io.h

```c

#ifndef RECEIVER_IO_H
#define RECEIVER_IO_H

#include <stdint.h>

/* Receive one session's file through the relay and store it at file_path. */
int receiver_receive_file(const char *session,
                          const char *relay,
                          uint16_t port,
                          const char *file_path);

#endif
```

### relay_io.c

```c

#define _POSIX_C_SOURCE 200809L

#include "relay_io.h"
#include "receiver_gbn.h"

#include <errno.h>
#include <math.h>
#include <limits.h>
#include <netdb.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

/* Read monotonic time in milliseconds for retransmission and idle deadlines. */
static int monotonic_now_ms(uint64_t *now_ms)
{
    struct timespec current_time;

    /* Excluded: clock_gettime failure depends on a host clock-system error. */
    if (clock_gettime(CLOCK_MONOTONIC, &current_time) != 0) { /* GCOVR_EXCL_START */
        return 0;
        /* GCOVR_EXCL_STOP */
    }
    *now_ms = (uint64_t)current_time.tv_sec * UINT64_C(1000) +
              (uint64_t)current_time.tv_nsec / UINT64_C(1000000);
    return 1;
}

/* Wait for socket readability until a monotonic deadline, retrying interruptions. */
static int wait_for_readable(int socket_fd, uint64_t timeout_ms)
{
    uint64_t now_ms;
    uint64_t deadline_ms;
    int poll_once = 1;

    /* Excluded: this path requires the operating system clock call to fail. */
    if (!monotonic_now_ms(&now_ms)) { /* GCOVR_EXCL_START */
        return -1;
        /* GCOVR_EXCL_STOP */
    }
    deadline_ms = now_ms > UINT64_MAX - timeout_ms ? UINT64_MAX : now_ms + timeout_ms;

    while (poll_once || now_ms < deadline_ms) {
        struct pollfd descriptor = {socket_fd, POLLIN, 0};
        uint64_t remaining_ms;
        int poll_timeout;
        int poll_status;

        poll_once = 0;
        /* Excluded: this path requires the operating system clock call to fail. */
        if (!monotonic_now_ms(&now_ms)) { /* GCOVR_EXCL_START */
            return -1;
            /* GCOVR_EXCL_STOP */
        }
        if (now_ms >= deadline_ms) {
            poll_timeout = 0;
        } else {
            remaining_ms = deadline_ms - now_ms;
            poll_timeout = remaining_ms > (uint64_t)INT_MAX
                ? INT_MAX : (int)remaining_ms;
        }
        poll_status = poll(&descriptor, 1, poll_timeout);
        /* Excluded: poll failures (other than EINTR) are environmental OS errors. */
        if (poll_status < 0) { /* GCOVR_EXCL_START */
            if (errno == EINTR) {
                continue;
            }
            return -1;
            /* GCOVR_EXCL_STOP */
        }
        if (poll_status == 0) {
            return 0;
        }
        /* Excluded branch: readiness/error combinations are determined by the OS. */
        if ((descriptor.revents & POLLIN) != 0) { /* GCOVR_EXCL_BR_LINE */
            return 1;
        }
        /* Excluded: these poll error flags require a socket/system error. */
        if ((descriptor.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0) { /* GCOVR_EXCL_START */
            return -1;
            /* GCOVR_EXCL_STOP */
        }
    }
    return 0; /* GCOVR_EXCL_LINE: unexpected poll event flags are OS-controlled. */
}

/* Resolve the relay and connect a UDP socket to one of its available addresses. */
int relay_client_open(relay_client_t *client, const char *relay, uint16_t port)
{
    struct addrinfo hints = {0};
    struct addrinfo *addresses = NULL;
    struct addrinfo *address;
    char service[6];
    int address_status;
    int last_error = 0;

    if (client == NULL || relay == NULL) {
        return 0;
    }
    client->socket_fd = -1;
    client->last_now_ms = 0;
    client->clock_failed = 0;
    (void)snprintf(service, sizeof(service), "%u", (unsigned int)port);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_DGRAM;
    address_status = getaddrinfo(relay, service, &hints, &addresses);
    /* Excluded: resolver failures depend on DNS and host networking conditions. */
    if (address_status != 0) { /* GCOVR_EXCL_START */
        fprintf(stderr, "Cannot resolve relay '%s': %s\n", relay, gai_strerror(address_status));
        return 0;
        /* GCOVR_EXCL_STOP */
    }

    /* Excluded branch: the number of addresses is controlled by the host resolver. */
    for (address = addresses; address != NULL; address = address->ai_next) { /* GCOVR_EXCL_BR_LINE */
        int candidate = socket(address->ai_family, address->ai_socktype, address->ai_protocol);
        /* Excluded: socket allocation failures are operating-system resource errors. */
        if (candidate < 0) { /* GCOVR_EXCL_START */
            last_error = errno;
            continue;
            /* GCOVR_EXCL_STOP */
        }
        /* Excluded branch: connect outcomes depend on host networking and OS state. */
        if (connect(candidate, address->ai_addr, address->ai_addrlen) == 0) { /* GCOVR_EXCL_BR_LINE */
            client->socket_fd = candidate;
            break;
        }
        /* Excluded: only reached when the operating system rejects connect(). */
        /* GCOVR_EXCL_START */
        last_error = errno;
        (void)close(candidate);
        /* GCOVR_EXCL_STOP */
    }
    freeaddrinfo(addresses);
    /* Excluded: all-address connection failure is environment-dependent. */
    if (client->socket_fd < 0) { /* GCOVR_EXCL_START */
        fprintf(stderr, "Cannot connect UDP socket to relay '%s': %s\n",
                relay, strerror(last_error));
        return 0;
        /* GCOVR_EXCL_STOP */
    }
    return 1;
}

/* Close the client's socket, if open, and mark the client as disconnected. */
void relay_client_close(relay_client_t *client)
{
    if (client != NULL && client->socket_fd >= 0) {
        (void)close(client->socket_fd);
        client->socket_fd = -1;
    }
}

/* Send a registration request and retry until the relay answers or attempts expire. */
static int register_hello(relay_client_t *client, const char *hello, size_t hello_length)
{
    int attempt;

    for (attempt = 0; attempt < 5; attempt++) {
        ssize_t sent;

        /* Excluded branch: EINTR during send is a signal-driven OS condition. */
        do {
            sent = send(client->socket_fd, hello, hello_length, 0);
        } while (sent < 0 && errno == EINTR); /* GCOVR_EXCL_BR_LINE */
        /* Excluded: UDP send failures or short sends are operating-system errors. */
        if (sent < 0 || (size_t)sent != hello_length) { /* GCOVR_EXCL_START */
            fprintf(stderr, "Could not send relay registration: %s\n", strerror(errno));
            return 0;
            /* GCOVR_EXCL_STOP */
        }

        {
            int receive_retry = 1;

            while (receive_retry) {
                char reply[256];
                ssize_t reply_length;
                int ready = wait_for_readable(client->socket_fd, UINT64_C(1000));

                /* Excluded: poll reports only environmental socket/system errors here. */
                if (ready < 0) { /* GCOVR_EXCL_START */
                    fprintf(stderr, "Network error waiting for relay registration: %s\n",
                            strerror(errno));
                    return 0;
                    /* GCOVR_EXCL_STOP */
                }
                if (ready == 0) {
                    break;
                }
                reply_length = recv(client->socket_fd, reply, sizeof(reply), 0);
                receive_retry = 0;
                /* Excluded: recv errors other than EINTR are operating-system failures. */
                if (reply_length < 0) { /* GCOVR_EXCL_START */
                    if (errno == EINTR) {
                        receive_retry = 1;
                        continue;
                    }
                    fprintf(stderr, "Could not receive relay registration reply: %s\n",
                            strerror(errno));
                    return 0;
                    /* GCOVR_EXCL_STOP */
                }
                if (reply_length == 2 && memcmp(reply, "OK", 2) == 0) {
                    return 1;
                }
                if (reply_length >= 4 && memcmp(reply, "ERR ", 4) == 0) {
                    fprintf(stderr, "Relay refused registration: %.*s\n",
                            (int)(reply_length - 4), reply + 4);
                    return 0;
                }
                fprintf(stderr, "Relay returned an invalid registration reply.\n");
                return 0;
            }
        }
    }

    fprintf(stderr, "Relay did not answer registration after five attempts.\n");
    return 0;
}

/* Register this client with the relay as the receiver for a session. */
int relay_client_register_receiver(relay_client_t *client, const char *session)
{
    char hello[48];
    int hello_length;

    if (client == NULL || client->socket_fd < 0 || !receiver_session_valid(session)) {
        return 0;
    }
    hello_length = snprintf(hello, sizeof(hello), "HELLO %s recv", session);
    /* Excluded: validated session length makes snprintf failure/truncation unreachable. */
    if (hello_length < 0 || (size_t)hello_length >= sizeof(hello)) { /* GCOVR_EXCL_START */
        return 0;
        /* GCOVR_EXCL_STOP */
    }
    return register_hello(client, hello, (size_t)hello_length);
}

/* Register as sender and provide the relay's loss, corruption, and duplication rates. */
int relay_client_register_sender(relay_client_t *client,
                                 const char *session,
                                 double loss,
                                 double corrupt,
                                 double duplicate)
{
    char hello[128];
    int hello_length;

    if (client == NULL || client->socket_fd < 0 || !receiver_session_valid(session) ||
        !isfinite(loss) || !isfinite(corrupt) || !isfinite(duplicate) ||
        loss < 0.0 || loss > 0.5 || corrupt < 0.0 || corrupt > 0.5 ||
        duplicate < 0.0 || duplicate > 0.5) {
        return 0;
    }
    hello_length = snprintf(hello, sizeof(hello), "HELLO %s send %.15g %.15g %.15g",
                            session, loss, corrupt, duplicate);
    /* Excluded: validated inputs fit the fixed registration buffer. */
    if (hello_length < 0 || (size_t)hello_length >= sizeof(hello)) { /* GCOVR_EXCL_START */
        return 0;
        /* GCOVR_EXCL_STOP */
    }
    return register_hello(client, hello, (size_t)hello_length);
}

/* Send one complete encoded protocol datagram to the connected relay. */
int relay_client_send_datagram(relay_client_t *client,
                               const unsigned char *datagram,
                               size_t datagram_length)
{
    ssize_t sent;

    if (client == NULL || client->socket_fd < 0 || datagram == NULL) {
        return -1;
    }
    /* Excluded branch: EINTR during send depends on signal delivery by the OS. */
    do {
        sent = send(client->socket_fd, datagram, datagram_length, 0);
    } while (sent < 0 && errno == EINTR); /* GCOVR_EXCL_BR_LINE */
    /* Excluded branch: EINTR and partial UDP sends are OS/network conditions. */
    return sent >= 0 && (size_t)sent == datagram_length ? 0 : -1; /* GCOVR_EXCL_BR_LINE */
}

/* Wait for and decode one relay datagram, distinguishing timeout and invalid input. */
relay_io_result_t relay_client_receive_packet(relay_client_t *client,
                                              uint64_t timeout_ms,
                                              packet_t *packet)
{
    int ready;
    ssize_t received;

    if (client == NULL || client->socket_fd < 0 || client->clock_failed) {
        return RELAY_IO_ERROR;
    }
    ready = wait_for_readable(client->socket_fd, timeout_ms);
    /* Excluded: wait_for_readable returns errors only for host clock/socket failures. */
    if (ready < 0) { /* GCOVR_EXCL_START */
        return RELAY_IO_ERROR;
        /* GCOVR_EXCL_STOP */
    }
    if (ready == 0) {
        return RELAY_IO_TIMEOUT;
    }
    received = recv(client->socket_fd, client->datagram, sizeof(client->datagram), 0);
    /* Excluded: recv errors depend on external socket state and OS behavior. */
    if (received < 0) { /* GCOVR_EXCL_START */
        return errno == EINTR ? RELAY_IO_TIMEOUT : RELAY_IO_ERROR;
        /* GCOVR_EXCL_STOP */
    }
    if (!packet_decode(client->datagram, (size_t)received, packet)) {
        return RELAY_IO_INVALID;
    }
    return RELAY_IO_PACKET;
}

/* Return the latest monotonic time, preserving the last value if the clock fails. */
uint64_t relay_client_now(relay_client_t *client)
{
    uint64_t now_ms;

    if (client == NULL) {
        return 0;
    }
    /* Excluded: clock_gettime failure cannot be induced reliably in unit tests. */
    if (!monotonic_now_ms(&now_ms)) { /* GCOVR_EXCL_START */
        client->clock_failed = 1;
        return client->last_now_ms;
        /* GCOVR_EXCL_STOP */
    }
    client->last_now_ms = now_ms;
    return now_ms;
}

/* Report whether a previous monotonic-clock read succeeded. */
int relay_client_clock_ok(const relay_client_t *client)
{
    return client != NULL && !client->clock_failed;
}
```

### relay_io.h

```c

#ifndef RELAY_IO_H
#define RELAY_IO_H

#include "packet.h"

#include <stddef.h>
#include <stdint.h>

typedef struct {
    int socket_fd;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE + 1U];
    uint64_t last_now_ms;
    int clock_failed;
} relay_client_t;

typedef enum {
    RELAY_IO_ERROR = -1,
    RELAY_IO_TIMEOUT = 0,
    RELAY_IO_PACKET = 1,
    RELAY_IO_INVALID = 2
} relay_io_result_t;

/* Resolve the relay and open a connected UDP socket. */
int relay_client_open(relay_client_t *client, const char *relay, uint16_t port);
/* Close the socket if it is open and mark the client as disconnected. */
void relay_client_close(relay_client_t *client);
/* Register this endpoint as a receiver for the given session. */
int relay_client_register_receiver(relay_client_t *client, const char *session);
/* Register this endpoint as a sender with relay impairment probabilities. */
int relay_client_register_sender(relay_client_t *client,
                                 const char *session,
                                 double loss,
                                 double corrupt,
                                 double duplicate);
/* Send one protocol datagram over the connected relay socket. */
int relay_client_send_datagram(relay_client_t *client,
                               const unsigned char *datagram,
                               size_t datagram_length);
/* Wait for a datagram and return packet, invalid-data, timeout, or I/O status. */
relay_io_result_t relay_client_receive_packet(relay_client_t *client,
                                              uint64_t timeout_ms,
                                              packet_t *packet);
/* Read monotonic time in milliseconds and retain the last value on failure. */
uint64_t relay_client_now(relay_client_t *client);
/* Check whether the client's last clock read succeeded. */
int relay_client_clock_ok(const relay_client_t *client);

#endif
```

### sender_gbn.c

```c

#include "sender_gbn.h"

#include <string.h>

/* Add timeout durations without overflowing the monotonic millisecond counter. */
static uint64_t add_saturated(uint64_t value, uint64_t amount)
{
    return value > UINT64_MAX - amount ? UINT64_MAX : value + amount;
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
```

### sender_gbn.h

```c

#ifndef SENDER_GBN_H
#define SENDER_GBN_H

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

/* Initialize sender state, returning zero when the window or timeout is invalid. */
int sender_state_init(sender_state_t *state,
                      uint32_t window_size,
                      uint64_t timeout_ms);
/* Return whether the sender can accept another DATA packet. */
int sender_can_accept_data(const sender_state_t *state);
/* Queue one DATA packet and return any immediate send or timer action. */
sender_status_t sender_on_data(sender_state_t *state,
                               const unsigned char *payload,
                               size_t payload_length,
                               uint64_t now_ms,
                               sender_action_t *action);
/* Signal input EOF and queue FIN when all DATA packets have been acknowledged. */
sender_status_t sender_on_eof(sender_state_t *state,
                              uint64_t now_ms,
                              sender_action_t *action);
/* Process a cumulative ACK and return retransmission/timer state changes. */
sender_status_t sender_on_ack(sender_state_t *state,
                              uint32_t next_expected,
                              uint64_t now_ms,
                              sender_action_t *action);
/* Handle an expired retransmission timer by retrying outstanding packets. */
sender_status_t sender_on_timeout(sender_state_t *state,
                                  uint64_t now_ms,
                                  sender_action_t *action);

#endif
```

### sender_io.c

```c

#include "sender_io.h"

#include "receiver_gbn.h"
#include "relay_io.h"
#include "sender_gbn.h"

#include <errno.h>
#include <stdio.h>
#include <string.h>

#ifdef TEST
enum {
    SENDER_IO_TEST_NORMAL = 0,
    SENDER_IO_TEST_DISARM_TIMER_AFTER_FIN = 1,
    SENDER_IO_TEST_NONEMPTY_ACK = 2,
    SENDER_IO_TEST_DATA_LIMIT = 3,
    SENDER_IO_TEST_FIN_SEQUENCE_LIMIT = 4
};

static int sender_io_test_mode;

/* Set a test-only condition that forces a sender network guard path. */
void sender_io_test_set_mode(int mode)
{
    sender_io_test_mode = mode;
}

const char *sender_io_test_input_path(void)
{
    return sender_io_test_mode == SENDER_IO_TEST_DATA_LIMIT ? "/dev/zero" : "/dev/null";
}

#endif

/* Encode and transmit every packet requested by the sender state machine. */
static int send_actions(relay_client_t *client, const sender_action_t *action)
{
    size_t index;

    for (index = 0; index < action->send_count; index++) {
        unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
        size_t datagram_length;

        if (!packet_encode(&action->packets[index], datagram, sizeof(datagram),
                           &datagram_length)) {
            return 0;
        }
        /* Excluded: socket send errors depend on external OS/network conditions. */
        if (relay_client_send_datagram(client, datagram, datagram_length) != 0) { /* GCOVR_EXCL_START */
            return 0;
            /* GCOVR_EXCL_STOP */
        }
    }
    return 1;
}

#ifdef TEST
int sender_io_test_rejects_invalid_action(void)
{
    relay_client_t client = {-1, {0}, 0, 0};
    sender_action_t action = {0};

    action.send_count = 1;
    action.packets[0].type = (packet_type_t)3;
    return !send_actions(&client, &action);
}
#endif

/* Read input, drive the sender state machine, and exchange packets with the relay. */
static int transfer_file(FILE *input,
                         relay_client_t *client,
                         uint32_t window_size,
                         uint64_t timeout_ms)
{
    sender_state_t state;

    if (!sender_state_init(&state, window_size, timeout_ms)) {
        fprintf(stderr, "Invalid sender window or timeout.\n");
        return 2;
    }
#ifdef TEST
    if (sender_io_test_mode == SENDER_IO_TEST_DATA_LIMIT) {
        state.base = SENDER_MAX_DATA_PACKETS;
        state.next = SENDER_MAX_DATA_PACKETS;
    } else if (sender_io_test_mode == SENDER_IO_TEST_FIN_SEQUENCE_LIMIT) {
        state.base = UINT32_MAX;
        state.next = UINT32_MAX;
    }
#endif

    for (;;) {
        while (sender_can_accept_data(&state)) {
            unsigned char payload[PACKET_MAX_PAYLOAD];
            size_t payload_length = fread(payload, 1, sizeof(payload), input);
            uint64_t now_ms;

            /* Excluded: input read failures depend on filesystem/device errors. */
            if (ferror(input)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not read input file.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
            now_ms = relay_client_now(client);
            /* Excluded: monotonic clock failure is a host operating-system error. */
            if (!relay_client_clock_ok(client)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not read monotonic clock.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
            if (payload_length > 0) {
                sender_action_t action;
                if (sender_on_data(&state, payload, payload_length, now_ms, &action) ==
                    SENDER_ERROR) {
                    fprintf(stderr, "Could not send DATA packet.\n");
                    return 2;
                }
                /* Excluded: packet-send failures are external socket/network errors. */
                if (!send_actions(client, &action)) { /* GCOVR_EXCL_START */
                    fprintf(stderr, "Could not send DATA packet.\n");
                    return 2;
                    /* GCOVR_EXCL_STOP */
                }
            }
            if (feof(input)) {
                sender_action_t action;
                if (sender_on_eof(&state, now_ms, &action) == SENDER_ERROR) {
                    fprintf(stderr, "Could not send FIN packet.\n");
                    return 2;
                }
                /* Excluded: packet-send failures are external socket/network errors. */
                if (!send_actions(client, &action)) { /* GCOVR_EXCL_START */
                    fprintf(stderr, "Could not send FIN packet.\n");
                    return 2;
                    /* GCOVR_EXCL_STOP */
                }
#ifdef TEST
                if (sender_io_test_mode == SENDER_IO_TEST_DISARM_TIMER_AFTER_FIN) {
                    state.timer_armed = 0;
                }
#endif
                break;
            }
            /* A zero-byte read without EOF or error is a stdio no-progress condition. */
            if (payload_length == 0) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Input file read made no progress.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
        }

        if (state.completed) {
            return 0;
        }
        if (!state.timer_armed) {
            fprintf(stderr, "Sender has outstanding packets without an active timer.\n");
            return 2;
        }

        {
            packet_t incoming;
            uint64_t now_ms = relay_client_now(client);
            uint64_t wait_ms;
            relay_io_result_t io_status;
            sender_action_t action;
            sender_status_t state_status;

            /* Excluded: this defensive branch requires a host clock failure. */
            if (!relay_client_clock_ok(client)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not read monotonic clock.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
            wait_ms = state.timer_due_ms > now_ms ? state.timer_due_ms - now_ms : 0;
            io_status = relay_client_receive_packet(client, wait_ms, &incoming);
            /* Excluded: receive errors depend on external socket/OS conditions. */
            if (io_status == RELAY_IO_ERROR) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Network error while waiting for ACK.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
            if (io_status == RELAY_IO_INVALID) {
                continue;
            }
            now_ms = relay_client_now(client);
            /* Excluded: this defensive branch requires a host clock failure. */
            if (!relay_client_clock_ok(client)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not read monotonic clock.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }

            if (io_status == RELAY_IO_PACKET) {
#ifdef TEST
                if (sender_io_test_mode == SENDER_IO_TEST_NONEMPTY_ACK &&
                    incoming.type == PACKET_ACK) {
                    incoming.payload_length = 1;
                }
#endif
                if (incoming.type != PACKET_ACK || incoming.payload_length != 0) {
                    continue;
                }
                state_status = sender_on_ack(&state, incoming.seq, now_ms, &action);
            } else {
                state_status = sender_on_timeout(&state, now_ms, &action);
            }
            if (state_status == SENDER_ERROR) {
                fprintf(stderr, "Sender failed after repeated timeouts or invalid state.\n");
                return 2;
            }
            /* Excluded: packet-send failures are external socket/network errors. */
            if (!send_actions(client, &action)) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not send protocol packet.\n");
                return 2;
                /* GCOVR_EXCL_STOP */
            }
        }
    }
}

/* Register as sender, open the input file, and run the retransmitting transfer. */
int sender_send_file(const char *session,
                     const char *relay,
                     uint16_t port,
                     const char *file_path,
                     uint32_t window_size,
                     uint64_t timeout_ms,
                     double loss,
                     double corrupt,
                     double duplicate)
{
    relay_client_t client = {-1, {0}, 0, 0};
    FILE *input;
    int status = 2;

    if (!receiver_session_valid(session)) {
        fprintf(stderr, "Invalid session name.\n");
        return 1;
    }
    if (relay == NULL || file_path == NULL) {
        return 2;
    }
    /* Excluded: DNS and UDP socket setup failures depend on host networking. */
    if (!relay_client_open(&client, relay, port)) { /* GCOVR_EXCL_START */
        return 2;
        /* GCOVR_EXCL_STOP */
    }
    if (!relay_client_register_sender(&client, session, loss, corrupt, duplicate)) {
        goto cleanup;
    }

    input = fopen(file_path, "rb");
    /* Excluded: file-open errors depend on external filesystem permissions/state. */
    if (input == NULL) { /* GCOVR_EXCL_START */
        fprintf(stderr, "Cannot open input file '%s': %s\n", file_path, strerror(errno));
        goto cleanup;
        /* GCOVR_EXCL_STOP */
    }
    status = transfer_file(input, &client, window_size, timeout_ms);
    {
        int close_status = fclose(input);
        if (status == 0) {
            /* Excluded: close failure depends on external filesystem/device state. */
            if (close_status != 0) { /* GCOVR_EXCL_START */
                fprintf(stderr, "Could not close input file cleanly.\n");
                status = 2;
                /* GCOVR_EXCL_STOP */
            }
        }
    }

cleanup:
    relay_client_close(&client);
    return status;
}
```

### sender_io.h

```c

#ifndef SENDER_IO_H
#define SENDER_IO_H

#include <stdint.h>

/* Send file_path to the receiver in session using the configured Go-Back-N window. */
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
```

## Tests Files
### lab-test.c

```c

#define _POSIX_C_SOURCE 200809L

#include <arpa/inet.h>
#include <errno.h>
#include <math.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

extern void __gcov_reset(void);
extern void __gcov_dump(void);

#include "harness/unity.h"
#include "../src/packet.h"
#include "../src/receiver_gbn.h"
#include "../src/receiver_io.h"
#include "../src/relay_io.h"
#include "../src/sender_gbn.h"
#include "../src/sender_io.h"

enum {
    RECEIVER_IO_TEST_NORMAL = 0,
    RECEIVER_IO_TEST_EXPIRED = 1,
    RECEIVER_IO_TEST_SEQUENCE_OVERFLOW = 2
};

enum {
    SENDER_IO_TEST_NORMAL = 0,
    SENDER_IO_TEST_DISARM_TIMER_AFTER_FIN = 1,
    SENDER_IO_TEST_NONEMPTY_ACK = 2,
    SENDER_IO_TEST_DATA_LIMIT = 3,
    SENDER_IO_TEST_FIN_SEQUENCE_LIMIT = 4
};

extern sender_status_t sender_test_queue_fin(sender_state_t *state,
                                              sender_action_t *action);
extern void receiver_io_test_set_mode(int mode);
extern void sender_io_test_set_mode(int mode);
extern int receiver_io_test_rejects_invalid_packet(void);
extern int sender_io_test_rejects_invalid_action(void);
extern const char *sender_io_test_input_path(void);

#define TEST_CHANNEL_CAPACITY 256U

typedef struct {
    unsigned char bytes[PACKET_MAX_DATAGRAM_SIZE];
    size_t length;
    int to_receiver;
} test_datagram_t;

typedef struct {
    test_datagram_t datagrams[TEST_CHANNEL_CAPACITY];
    size_t head;
    size_t count;
    uint64_t random_state;
    int impaired;
} test_channel_t;

/* Advance the deterministic test PRNG and return its next 32-bit value. */
static uint32_t test_random_next(test_channel_t *channel)
{
    uint64_t value = channel->random_state;

    value ^= value >> 12;
    value ^= value << 25;
    value ^= value >> 27;
    channel->random_state = value;
    return (uint32_t)((value * UINT64_C(2685821657736338717)) >> 32);
}

/* Append one encoded datagram and its direction to the simulated network queue. */
static int test_channel_enqueue(test_channel_t *channel,
                                const unsigned char *datagram,
                                size_t datagram_length,
                                int to_receiver)
{
    size_t tail;
    test_datagram_t *message;

    if (channel->count >= TEST_CHANNEL_CAPACITY ||
        datagram_length > PACKET_MAX_DATAGRAM_SIZE) {
        return 0;
    }
    tail = (channel->head + channel->count) % TEST_CHANNEL_CAPACITY;
    message = &channel->datagrams[tail];
    memcpy(message->bytes, datagram, datagram_length);
    message->length = datagram_length;
    message->to_receiver = to_receiver;
    channel->count++;
    return 1;
}

/* Encode a packet and optionally drop, corrupt, or duplicate it in transit. */
static int test_channel_send_packet(test_channel_t *channel,
                                    const packet_t *packet,
                                    int to_receiver)
{
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length;
    int duplicate;

    if (!packet_encode(packet, datagram, sizeof(datagram), &datagram_length)) {
        return 0;
    }
    if (channel->impaired && test_random_next(channel) % 100U < 20U) {
        return 1;
    }
    if (channel->impaired && test_random_next(channel) % 100U < 20U) {
        size_t byte_index = test_random_next(channel) % datagram_length;
        unsigned char bit = (unsigned char)(1U << (test_random_next(channel) % 8U));
        datagram[byte_index] ^= bit;
    }
    duplicate = channel->impaired && test_random_next(channel) % 100U < 20U;
    if (!test_channel_enqueue(channel, datagram, datagram_length, to_receiver)) {
        return 0;
    }
    return !duplicate ||
           test_channel_enqueue(channel, datagram, datagram_length, to_receiver);
}

/* Send each packet from a sender action into the simulated channel. */
static int test_channel_send_action(test_channel_t *channel,
                                    const sender_action_t *action)
{
    size_t index;

    for (index = 0; index < action->send_count; index++) {
        if (!test_channel_send_packet(channel, &action->packets[index], 1)) {
            return 0;
        }
    }
    return 1;
}

/* Run a complete in-memory sender/receiver transfer, optionally with impairments. */
static int test_run_file_transfer(FILE *input,
                                  uint64_t seed,
                                  int impaired,
                                  unsigned char *delivered,
                                  size_t delivered_capacity,
                                  size_t *delivered_length,
                                  size_t *delivered_packets)
{
    test_channel_t channel = {0};
    sender_state_t sender;
    receiver_state_t receiver;
    uint64_t now_ms = 0;
    int eof = 0;
    size_t step;

    channel.random_state = seed == 0 ? UINT64_C(1) : seed;
    channel.impaired = impaired;
    *delivered_length = 0;
    *delivered_packets = 0;
    if (!sender_state_init(&sender, 4, 20)) {
        return 0;
    }
    receiver_state_init(&receiver, 0);

    for (step = 0; step < 200000; step++) {
        while (!eof && sender_can_accept_data(&sender)) {
            unsigned char payload[PACKET_MAX_PAYLOAD];
            size_t payload_length = fread(payload, 1, sizeof(payload), input);
            sender_action_t action;

            if (ferror(input)) {
                return 0;
            }
            if (payload_length > 0) {
                if (sender_on_data(&sender, payload, payload_length,
                                   now_ms, &action) != SENDER_ACTIVE ||
                    !test_channel_send_action(&channel, &action)) {
                    return 0;
                }
            }
            if (feof(input)) {
                eof = 1;
                if (sender_on_eof(&sender, now_ms, &action) == SENDER_ERROR ||
                    !test_channel_send_action(&channel, &action)) {
                    return 0;
                }
                break;
            }
            if (payload_length == 0) {
                return 0;
            }
        }

        if (channel.count == 0) {
            sender_action_t action;

            if (sender.completed) {
                return 1;
            }
            if (!sender.timer_armed) {
                return 0;
            }
            now_ms = sender.timer_due_ms;
            if (sender_on_timeout(&sender, now_ms, &action) != SENDER_ACTIVE ||
                !test_channel_send_action(&channel, &action)) {
                return 0;
            }
            continue;
        }

        {
            test_datagram_t message = channel.datagrams[channel.head];
            packet_t packet;

            channel.head = (channel.head + 1) % TEST_CHANNEL_CAPACITY;
            channel.count--;
            if (!packet_decode(message.bytes, message.length, &packet)) {
                continue;
            }
            if (message.to_receiver) {
                receiver_action_t action;
                receiver_status_t status = receiver_on_packet(
                    &receiver, &packet, now_ms, &action);

                if (status == RECEIVER_ERROR) {
                    return 0;
                }
                if (action.deliver_payload) {
                    if (action.payload_length > delivered_capacity - *delivered_length) {
                        return 0;
                    }
                    memcpy(delivered + *delivered_length, action.payload,
                           action.payload_length);
                    *delivered_length += action.payload_length;
                    (*delivered_packets)++;
                }
                if (action.send_ack) {
                    packet_t ack = {0};
                    ack.type = PACKET_ACK;
                    ack.seq = action.ack_sequence;
                    if (!test_channel_send_packet(&channel, &ack, 0)) {
                        return 0;
                    }
                }
            } else if (packet.type == PACKET_ACK) {
                sender_action_t action;
                sender_status_t status = sender_on_ack(
                    &sender, packet.seq, now_ms, &action);

                if (status == SENDER_ERROR ||
                    !test_channel_send_action(&channel, &action)) {
                    return 0;
                }
            }
        }
    }
    return 0;
}

/* Wrap bytes in a temporary input file and run the simulated transfer. */
static int test_simulate_bytes(const unsigned char *bytes,
                               size_t length,
                               uint64_t seed,
                               int impaired,
                               unsigned char *delivered,
                               size_t delivered_capacity,
                               size_t *delivered_length,
                               size_t *delivered_packets)
{
    FILE *input = tmpfile();
    int result;

    if (input == NULL || (length > 0 && fwrite(bytes, 1, length, input) != length)) {
        if (input != NULL) {
            (void)fclose(input);
        }
        return 0;
    }
    rewind(input);
    result = test_run_file_transfer(input, seed, impaired, delivered,
                                    delivered_capacity, delivered_length,
                                    delivered_packets);
    if (fclose(input) != 0) {
        return 0;
    }
    return result;
}

/* Compare IPv4 socket addresses by host and port. */
static int test_sockaddr_equal(const struct sockaddr_in *left,
                               const struct sockaddr_in *right)
{
    return left->sin_addr.s_addr == right->sin_addr.s_addr &&
           left->sin_port == right->sin_port;
}

/* Emulate the relay, including invalid packets and a deliberately dropped DATA ACK. */
static int test_relay_process(int socket_fd)
{
    struct sockaddr_in sender_address = {0};
    struct sockaddr_in receiver_address = {0};
    socklen_t sender_length = sizeof(sender_address);
    socklen_t receiver_length = sizeof(receiver_address);
    int have_sender = 0;
    int have_receiver = 0;
    int have_pending = 0;
    int dropped_data_ack = 0;
    int fin_seen = 0;
    unsigned char pending[PACKET_MAX_DATAGRAM_SIZE];
    size_t pending_length = 0;
    unsigned int iteration;

    for (iteration = 0; iteration < 1000; iteration++) {
        struct pollfd descriptor = {socket_fd, POLLIN, 0};
        unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE + 1U];
        struct sockaddr_in source = {0};
        socklen_t source_length = sizeof(source);
        ssize_t received;

        if (poll(&descriptor, 1, 10000) <= 0) {
            return 1;
        }
        received = recvfrom(socket_fd, datagram, sizeof(datagram), 0,
                            (struct sockaddr *)&source, &source_length);
        if (received < 0) {
            return 1;
        }
        if ((size_t)received >= 6 && memcmp(datagram, "HELLO ", 6) == 0) {
            const char *hello = (const char *)datagram;
            const char *terminator = memchr(datagram, '\0', (size_t)received);
            size_t hello_length = terminator == NULL ? (size_t)received
                                                     : (size_t)(terminator - hello);
            int is_receiver = hello_length >= 5 &&
                              memcmp(datagram + hello_length - 5, " recv", 5) == 0;
            const char reply[] = "OK";

            if (is_receiver) {
                receiver_address = source;
                receiver_length = source_length;
                have_receiver = 1;
            } else {
                sender_address = source;
                sender_length = source_length;
                have_sender = 1;
            }
            if (sendto(socket_fd, reply, sizeof(reply) - 1, 0,
                       (struct sockaddr *)&source, source_length) !=
                (ssize_t)(sizeof(reply) - 1)) {
                return 1;
            }
            if (have_sender && have_receiver) {
                const unsigned char invalid_datagram[] = {0xff};
                packet_t unexpected_packet = {0};
                unsigned char unexpected_datagram[PACKET_MAX_DATAGRAM_SIZE];
                size_t unexpected_length;

                unexpected_packet.type = PACKET_DATA;
                unexpected_packet.payload_length = 1;
                unexpected_packet.payload[0] = 'u';
                if (!packet_encode(&unexpected_packet, unexpected_datagram,
                                   sizeof(unexpected_datagram), &unexpected_length) ||
                    sendto(socket_fd, unexpected_datagram, unexpected_length, 0,
                           (struct sockaddr *)&sender_address, sender_length) !=
                        (ssize_t)unexpected_length) {
                    return 1;
                }
                (void)sendto(socket_fd, invalid_datagram, sizeof(invalid_datagram), 0,
                             (struct sockaddr *)&sender_address, sender_length);
                (void)sendto(socket_fd, invalid_datagram, sizeof(invalid_datagram), 0,
                             (struct sockaddr *)&receiver_address, receiver_length);
                {
                    packet_t ignored_ack = {0};
                    unsigned char ack_datagram[PACKET_HEADER_SIZE];
                    size_t ack_length;

                    ignored_ack.type = PACKET_ACK;
                    if (!packet_encode(&ignored_ack, ack_datagram,
                                       sizeof(ack_datagram), &ack_length) ||
                        sendto(socket_fd, ack_datagram, ack_length, 0,
                               (struct sockaddr *)&receiver_address,
                               receiver_length) != (ssize_t)ack_length) {
                        return 1;
                    }
                }
                if (have_pending) {
                    if (sendto(socket_fd, pending, pending_length, 0,
                               (struct sockaddr *)&receiver_address, receiver_length) !=
                        (ssize_t)pending_length) {
                        return 1;
                    }
                    have_pending = 0;
                }
            }
            continue;
        }

        if (have_sender && test_sockaddr_equal(&source, &sender_address)) {
            packet_t packet;

            if (packet_decode(datagram, (size_t)received, &packet) &&
                packet.type == PACKET_FIN) {
                fin_seen = 1;
            }
            if (!have_receiver) {
                memcpy(pending, datagram, (size_t)received);
                pending_length = (size_t)received;
                have_pending = 1;
                continue;
            }
            if (sendto(socket_fd, datagram, (size_t)received, 0,
                       (struct sockaddr *)&receiver_address, receiver_length) != received) {
                return 1;
            }
        } else if (have_receiver && test_sockaddr_equal(&source, &receiver_address)) {
            packet_t packet;
            int is_fin_ack = fin_seen &&
                             packet_decode(datagram, (size_t)received, &packet) &&
                             packet.type == PACKET_ACK;

            if (!dropped_data_ack && packet_decode(datagram, (size_t)received, &packet) &&
                packet.type == PACKET_ACK && packet.seq == 1) {
                dropped_data_ack = 1;
                continue;
            }
            if (have_sender && sendto(socket_fd, datagram, (size_t)received, 0,
                                      (struct sockaddr *)&sender_address,
                                      sender_length) != received) {
                return 1;
            }
            if (is_fin_ack) {
                return 0;
            }
        }
    }
    return 1;
}

/* Reply to a chosen number of registrations and optionally send a follow-up packet. */
static int test_registration_server(int socket_fd,
                                    const char *reply,
                                    unsigned int registration_count)
{
    unsigned int attempt;

    for (attempt = 0; attempt < registration_count; attempt++) {
        unsigned char request[128];
        struct sockaddr_in source = {0};
        socklen_t source_length = sizeof(source);
        ssize_t received = recvfrom(socket_fd, request, sizeof(request), 0,
                                    (struct sockaddr *)&source, &source_length);

        if (received < 0) {
            return 1;
        }
        {
            const char *wire_reply = reply;
            if (reply != NULL &&
                (strcmp(reply, "OK_PACKET") == 0 || strcmp(reply, "OK_DATA_MAX") == 0)) {
                wire_reply = "OK";
            }
            if (wire_reply != NULL &&
                sendto(socket_fd, wire_reply, strlen(wire_reply), 0,
                   (struct sockaddr *)&source, source_length) !=
                    (ssize_t)strlen(wire_reply)) {
                return 1;
            }
        }
        if (reply != NULL && strcmp(reply, "OK_PACKET") == 0) {
            packet_t packet = {0};
            unsigned char datagram[PACKET_HEADER_SIZE];
            size_t datagram_length;

            packet.type = PACKET_ACK;
            if (!packet_encode(&packet, datagram, sizeof(datagram), &datagram_length) ||
                sendto(socket_fd, datagram, datagram_length, 0,
                       (struct sockaddr *)&source, source_length) !=
                    (ssize_t)datagram_length) {
                return 1;
            }
        } else if (reply != NULL && strcmp(reply, "OK_DATA_MAX") == 0) {
            packet_t packet = {0};
            unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
            size_t datagram_length;

            packet.type = PACKET_DATA;
            packet.seq = UINT32_MAX;
            packet.payload_length = 1;
            packet.payload[0] = 'x';
            if (!packet_encode(&packet, datagram, sizeof(datagram), &datagram_length) ||
                sendto(socket_fd, datagram, datagram_length, 0,
                       (struct sockaddr *)&source, source_length) !=
                    (ssize_t)datagram_length) {
                return 1;
            }
        }
    }
    return 0;
}

/* Accept a sender registration and discard DATA until its retry limit is reached. */
static int test_drop_ack_server(int socket_fd)
{
    struct sockaddr_in sender_address = {0};
    socklen_t sender_length = sizeof(sender_address);
    unsigned char hello[128];
    ssize_t received;
    const char reply[] = "OK";
    unsigned int data_count = 0;

    received = recvfrom(socket_fd, hello, sizeof(hello), 0,
                        (struct sockaddr *)&sender_address, &sender_length);
    if (received < 0 ||
        sendto(socket_fd, reply, sizeof(reply) - 1, 0,
               (struct sockaddr *)&sender_address, sender_length) !=
            (ssize_t)(sizeof(reply) - 1)) {
        return 1;
    }

    while (data_count < SENDER_MAX_TIMEOUTS) {
        struct pollfd descriptor = {socket_fd, POLLIN, 0};
        unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
        packet_t packet;

        if (poll(&descriptor, 1, 5000) <= 0) {
            return 1;
        }
        received = recvfrom(socket_fd, datagram, sizeof(datagram), 0,
                            (struct sockaddr *)&sender_address, &sender_length);
        if (received < 0) {
            return 1;
        }
        if (packet_decode(datagram, (size_t)received, &packet) &&
            packet.type == PACKET_DATA) {
            data_count++;
        }
    }
    return 0;
}

/* Run a local UDP relay scenario and check the requested registration outcome. */
static int test_registration_result(const char *reply,
                                    unsigned int registration_count,
                                    int expected_result,
                                    int operation,
                                    uint32_t window_size)
{
    struct sockaddr_in relay_address = {0};
    socklen_t relay_address_length = sizeof(relay_address);
    relay_client_t client = {-1, {0}, 0, 0};
    int socket_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int server_status;
    int registration_result;
    pid_t server_pid;
    packet_t packet;

    if (socket_fd < 0) {
        return 0;
    }
    relay_address.sin_family = AF_INET;
    relay_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(socket_fd, (struct sockaddr *)&relay_address, sizeof(relay_address)) != 0 ||
        getsockname(socket_fd, (struct sockaddr *)&relay_address,
                    &relay_address_length) != 0) {
        (void)close(socket_fd);
        return 0;
    }
    server_pid = fork();
    if (server_pid < 0) {
        (void)close(socket_fd);
        return 0;
    }
    if (server_pid == 0) {
        int status;
        __gcov_reset();
        status = test_registration_server(socket_fd, reply, registration_count);
        __gcov_dump();
        _exit(status);
    }
    (void)close(socket_fd);
    if (!relay_client_open(&client, "127.0.0.1", ntohs(relay_address.sin_port))) {
        (void)waitpid(server_pid, &server_status, 0);
        return 0;
    }
    if (relay_client_receive_packet(&client, 0, &packet) != RELAY_IO_TIMEOUT) {
        relay_client_close(&client);
        return 0;
    }
    if (operation == 1) {
        relay_client_close(&client);
        registration_result = sender_send_file(
            "coveragetest", "127.0.0.1",
            ntohs(relay_address.sin_port),
            sender_io_test_input_path(),
            window_size, 1, 0.5, 0.5, 0.5);
    } else if (operation == 2) {
        relay_client_close(&client);
        registration_result = receiver_receive_file("coveragetest", "127.0.0.1",
                                                     ntohs(relay_address.sin_port), "/dev/null");
    } else {
        registration_result = relay_client_register_receiver(&client, "coveragetest");
        if (registration_result && reply != NULL && strcmp(reply, "OK_PACKET") == 0 &&
            relay_client_receive_packet(&client, UINT64_MAX, &packet) != RELAY_IO_PACKET) {
            relay_client_close(&client);
            return 0;
        }
        relay_client_close(&client);
    }
    if (waitpid(server_pid, &server_status, 0) != server_pid ||
        !WIFEXITED(server_status) || WEXITSTATUS(server_status) != 0) {
        return 0;
    }
    return registration_result == expected_result;
}

/* Verify packet checksum/encode/decode APIs reject invalid pointers and fields. */
static void test_packet_api_rejects_invalid_arguments(void)
{
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE + 1U] = {0};
    size_t datagram_length = 0;

    TEST_ASSERT_EQUAL_INT(0, packet_checksum(NULL, 1));
    TEST_ASSERT_EQUAL_INT(0xffff, packet_checksum(NULL, 0));
    TEST_ASSERT_FALSE(packet_encode(NULL, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_FALSE(packet_encode(&packet, NULL, sizeof(datagram), &datagram_length));
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, sizeof(datagram), NULL));

    packet.type = (packet_type_t)3;
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    packet.type = PACKET_ACK;
    packet.payload_length = 1;
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    packet.payload_length = PACKET_MAX_PAYLOAD + 1U;
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    packet.payload_length = 0;
    TEST_ASSERT_FALSE(packet_encode(&packet, datagram, PACKET_HEADER_SIZE - 1,
                                   &datagram_length));
    TEST_ASSERT_FALSE(packet_decode(NULL, 0, &decoded));
    TEST_ASSERT_FALSE(packet_decode(datagram, PACKET_HEADER_SIZE, NULL));

    packet.type = PACKET_ACK;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    datagram[8] = 0x04;
    datagram[9] = 0x01;
    TEST_ASSERT_FALSE(packet_decode(datagram, sizeof(datagram), &decoded));

    packet.type = PACKET_DATA;
    packet.payload_length = 1;
    packet.payload[0] = 'x';
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    datagram[0] = PACKET_ACK;
    datagram[2] = 0;
    datagram[3] = 0;
    {
        uint16_t checksum = htons(packet_checksum(datagram, datagram_length));
        memcpy(datagram + 2, &checksum, sizeof(checksum));
    }
    TEST_ASSERT_FALSE(packet_decode(datagram, datagram_length, &decoded));
}

/* Cover receiver argument/state guards, sequence overflow, and deadline saturation. */
static void test_receiver_state_rejects_invalid_transitions_and_saturates(void)
{
    receiver_state_t state;
    receiver_action_t action;
    packet_t packet = {0};

    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid(NULL));
    receiver_state_init(NULL, 0);
    TEST_ASSERT_EQUAL_UINT64(0, receiver_next_deadline(NULL));
    receiver_state_init(&state, UINT64_MAX - RECEIVER_IDLE_TIMEOUT_MS + 1);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, receiver_next_deadline(&state));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(NULL, &packet, 0, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, NULL, 0, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 0, NULL));
    packet.type = (packet_type_t)3;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 0, &action));
    packet.type = PACKET_ACK;
    packet.payload_length = 1;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 0, &action));
    packet.type = PACKET_DATA;
    packet.payload_length = PACKET_MAX_PAYLOAD + 1U;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 0, &action));

    packet.payload_length = 0;
    receiver_state_init(&state, 10);
    packet.type = PACKET_ACK;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 20, &action));
    TEST_ASSERT_EQUAL_UINT64(10, state.last_valid_ms);
    packet.type = PACKET_DATA;
    packet.seq = state.expected + 2;
    packet.payload_length = 0;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 25, &action));
    TEST_ASSERT_FALSE(action.deliver_payload);
    packet.type = PACKET_FIN;
    packet.seq = 1;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 30, &action));
    TEST_ASSERT_TRUE(action.send_ack);

    receiver_state_init(&state, 50);
    packet.type = PACKET_DATA;
    packet.seq = 0;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 51, &action));
    TEST_ASSERT_FALSE(action.deliver_payload);

    state.expected = UINT32_MAX;
    packet.type = PACKET_DATA;
    packet.seq = UINT32_MAX;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 40, &action));
    packet.type = PACKET_FIN;
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR,
                          receiver_on_packet(&state, &packet, 40, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR, receiver_on_timeout(NULL, 0, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_ERROR, receiver_on_timeout(&state, 0, NULL));
    receiver_state_init(&state, 100);
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_timeout(&state, 99, &action));
}

/* Cover sender argument/state guards, packet limits, and timer overflow behavior. */
static void test_sender_state_rejects_invalid_transitions_and_saturates(void)
{
    sender_state_t state;
    sender_action_t action;
    unsigned char payload = 'x';

    TEST_ASSERT_FALSE(sender_state_init(NULL, 1, 1));
    TEST_ASSERT_FALSE(sender_can_accept_data(NULL));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_data(NULL, &payload, 1, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_ack(NULL, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(NULL, 0, &action));
    TEST_ASSERT_TRUE(sender_state_init(&state, 1, 10));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_timeout(&state, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_data(&state, NULL, 1, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_data(&state, &payload, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR,
                          sender_on_data(&state, &payload, PACKET_MAX_PAYLOAD + 1U,
                                         0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_data(&state, &payload, 1, 0, NULL));

    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&state, &payload, 1, UINT64_MAX - 2, &action));
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, state.timer_due_ms);
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_eof(NULL, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_eof(&state, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_ack(&state, 0, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(&state, 0, NULL));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&state, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_ack(&state, state.next + 1, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_timeout(&state, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(&state, 0, NULL));

    state.base = SENDER_MAX_DATA_PACKETS;
    state.next = SENDER_MAX_DATA_PACKETS;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR,
                          sender_on_data(&state, &payload, 1, 0, &action));
    state.next = UINT32_MAX;
    state.base = UINT32_MAX;
    state.timer_armed = 0;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_eof(&state, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&state, SENDER_MAX_WINDOW, 1));
    state.failed = 1;
    TEST_ASSERT_FALSE(sender_can_accept_data(&state));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_eof(&state, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_ack(&state, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(&state, 0, &action));
    state.failed = 0;
    state.eof = 1;
    TEST_ASSERT_FALSE(sender_can_accept_data(&state));
    state.eof = 0;
    state.fin_sent = 1;
    TEST_ASSERT_FALSE(sender_can_accept_data(&state));
    state.fin_sent = 0;

    state.next = SENDER_MAX_WINDOW + 1;
    state.timer_armed = 1;
    state.timer_due_ms = 0;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_on_timeout(&state, 1, &action));
    TEST_ASSERT_TRUE(state.failed);

    TEST_ASSERT_TRUE(sender_state_init(&state, 1, 1));
    state.eof = 1;
    state.fin_sent = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_eof(&state, 0, &action));
    state.fin_sent = 0;
    state.next = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_eof(&state, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&state, 1, 1));
    state.base = UINT32_MAX - 1;
    state.next = UINT32_MAX;
    state.eof = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR,
                          sender_on_ack(&state, UINT32_MAX, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&state, 2, 1));
    state.base = 0;
    state.next = 2;
    state.fin_sent = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&state, 1, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&state, 1, 1));
    memset(&action, 0, sizeof(action));
    action.send_count = SENDER_MAX_WINDOW;
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR, sender_test_queue_fin(&state, &action));
    TEST_ASSERT_TRUE(state.failed);

    state.failed = 0;
    state.completed = 1;
    TEST_ASSERT_EQUAL_INT(SENDER_DONE, sender_on_eof(&state, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_DONE, sender_on_ack(&state, 0, 0, &action));
    TEST_ASSERT_EQUAL_INT(SENDER_DONE, sender_on_timeout(&state, 0, &action));
}

/* Create temporary files and processes to test an end-to-end UDP file transfer. */
static void test_run_network_transfer(const unsigned char *payload, size_t payload_length)
{
    unsigned char received[PACKET_MAX_PAYLOAD];
    char input_path[] = "/tmp/cs525-input-XXXXXX";
    char output_path[] = "/tmp/cs525-output-XXXXXX";
    struct sockaddr_in relay_address = {0};
    socklen_t relay_address_length = sizeof(relay_address);
    int relay_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int input_fd;
    int output_fd;
    pid_t relay_pid;
    pid_t sender_pid;
    int relay_status;
    int sender_status;
    int receiver_status;
    int input_ok;
    FILE *output;
    size_t received_length;

    TEST_ASSERT_TRUE(relay_fd >= 0);
    relay_address.sin_family = AF_INET;
    relay_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    TEST_ASSERT_EQUAL_INT(0, bind(relay_fd, (struct sockaddr *)&relay_address,
                                  sizeof(relay_address)));
    TEST_ASSERT_EQUAL_INT(0, getsockname(relay_fd, (struct sockaddr *)&relay_address,
                                         &relay_address_length));
    input_fd = mkstemp(input_path);
    output_fd = mkstemp(output_path);
    TEST_ASSERT_TRUE(input_fd >= 0 && output_fd >= 0);
    input_ok = payload_length == 0 ||
               write(input_fd, payload, payload_length) == (ssize_t)payload_length;
    TEST_ASSERT_EQUAL_INT(0, close(input_fd));
    TEST_ASSERT_EQUAL_INT(0, close(output_fd));

    relay_pid = fork();
    TEST_ASSERT_TRUE(relay_pid >= 0);
    if (relay_pid == 0) {
        int status;
        __gcov_reset();
        status = test_relay_process(relay_fd);
        __gcov_dump();
        _exit(status);
    }
    TEST_ASSERT_EQUAL_INT(0, close(relay_fd));
    sender_pid = fork();
    TEST_ASSERT_TRUE(sender_pid >= 0);
    if (sender_pid == 0) {
        int status;
        __gcov_reset();
        status = sender_send_file("coveragetest", "127.0.0.1",
                                  ntohs(relay_address.sin_port), input_path,
                                  1, 20, 0.0, 0.0, 0.0);
        __gcov_dump();
        _exit(status);
    }

    receiver_status = receiver_receive_file("coveragetest", "127.0.0.1",
                                             ntohs(relay_address.sin_port), output_path);
    TEST_ASSERT_EQUAL_INT(sender_pid, waitpid(sender_pid, &sender_status, 0));
    TEST_ASSERT_EQUAL_INT(relay_pid, waitpid(relay_pid, &relay_status, 0));
    TEST_ASSERT_TRUE(input_ok);
    TEST_ASSERT_EQUAL_INT(0, receiver_status);
    TEST_ASSERT_TRUE(WIFEXITED(sender_status));
    TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(sender_status));
    TEST_ASSERT_TRUE(WIFEXITED(relay_status));
    TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(relay_status));

    output = fopen(output_path, "rb");
    TEST_ASSERT_NOT_NULL(output);
    received_length = fread(received, 1, sizeof(received), output);
    TEST_ASSERT_EQUAL_INT(0, fclose(output));
    TEST_ASSERT_EQUAL_UINT64(payload_length, received_length);
    TEST_ASSERT_TRUE(payload_length == 0 ||
                     memcmp(payload, received, payload_length) == 0);
    TEST_ASSERT_EQUAL_INT(0, unlink(input_path));
    TEST_ASSERT_EQUAL_INT(0, unlink(output_path));
}

/* Verify a nonempty file transfers through the local test relay. */
void test_network_transfer_through_loopback_relay(void)
{
    const unsigned char payload[] = "loopback coverage";

    test_run_network_transfer(payload, sizeof(payload));
}

/* Verify an empty file completes without sending a DATA packet. */
void test_empty_network_transfer_sends_only_fin(void)
{
    test_run_network_transfer(NULL, 0);
}

/* Verify a full-sized payload transfers correctly through the network path. */
void test_exact_payload_network_transfer(void)
{
    unsigned char payload[PACKET_MAX_PAYLOAD];
    size_t index;

    for (index = 0; index < sizeof(payload); index++) {
        payload[index] = (unsigned char)(index * 29U);
    }
    test_run_network_transfer(payload, sizeof(payload));
}

/* Unity fixture hook: these tests do not require per-test setup. */
void setUp(void) {}
/* Unity fixture hook: tests own and release their temporary resources. */
void tearDown(void) {}

/* Check the checksum against the published RFC 1071 example. */
void test_checksum_matches_rfc1071_example(void)
{
    const unsigned char example[] = {0x00, 0x01, 0xf2, 0x03, 0xf4, 0xf5, 0xf6, 0xf7};
    TEST_ASSERT_EQUAL_INT(0x220d, packet_checksum(example, sizeof(example)));
}

/* Verify odd-length checksums and detection of a modified datagram byte. */
void test_checksum_handles_odd_length_and_detects_a_flipped_bit(void)
{
    const unsigned char odd_bytes[] = {0x01, 0x02, 0x03};
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length = 0;

    TEST_ASSERT_EQUAL_INT(0xfbfd, packet_checksum(odd_bytes, sizeof(odd_bytes)));
    packet.type = PACKET_DATA;
    packet.payload_length = sizeof(odd_bytes);
    memcpy(packet.payload, odd_bytes, sizeof(odd_bytes));
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_EQUAL_INT(0, packet_checksum(datagram, datagram_length));
    datagram[PACKET_HEADER_SIZE + 1] ^= 1;
    TEST_ASSERT_NOT_EQUAL(0, packet_checksum(datagram, datagram_length));
    TEST_ASSERT_FALSE(packet_decode(datagram, datagram_length, &decoded));
}

/* Verify session names accept only the allowed characters and length. */
void test_session_name_validation(void)
{
    TEST_ASSERT_TRUE(receiver_session_valid("jdoe-1"));
    TEST_ASSERT_TRUE(receiver_session_valid("abcdefghijklmnopqrstuvwxyz123456"));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid(""));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid("Bad_Name"));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid("jdoe{"));
    TEST_ASSERT_EQUAL_INT(0, receiver_session_valid("abcdefghijklmnopqrstuvwxyz1234567"));
}

/* Verify packet fields encode in network order and decode back with checksum checks. */
void test_packet_wire_format_round_trips_and_checks_checksum(void)
{
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length = 0;

    packet.type = PACKET_DATA;
    packet.seq = UINT32_C(0x01020304);
    packet.payload_length = 2;
    packet.payload[0] = 0xaa;
    packet.payload[1] = 0xbb;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_EQUAL_UINT64(PACKET_HEADER_SIZE + 2, datagram_length);
    TEST_ASSERT_EQUAL_INT(0, datagram[0]);
    TEST_ASSERT_EQUAL_INT(0, datagram[1]);
    TEST_ASSERT_EQUAL_INT(1, datagram[4]);
    TEST_ASSERT_EQUAL_INT(2, datagram[5]);
    TEST_ASSERT_EQUAL_INT(3, datagram[6]);
    TEST_ASSERT_EQUAL_INT(4, datagram[7]);
    TEST_ASSERT_EQUAL_INT(0, datagram[8]);
    TEST_ASSERT_EQUAL_INT(2, datagram[9]);
    TEST_ASSERT_TRUE(packet_decode(datagram, datagram_length, &decoded));
    TEST_ASSERT_EQUAL_INT(PACKET_DATA, decoded.type);
    TEST_ASSERT_EQUAL_UINT64(UINT32_C(0x01020304), decoded.seq);
    TEST_ASSERT_EQUAL_UINT64(2, decoded.payload_length);
    TEST_ASSERT_EQUAL_INT(0xaa, decoded.payload[0]);
    TEST_ASSERT_EQUAL_INT(0xbb, decoded.payload[1]);

    datagram[PACKET_HEADER_SIZE] ^= 1;
    TEST_ASSERT_EQUAL_INT(0, packet_decode(datagram, datagram_length, &decoded));
}

/* Verify the decoder rejects reserved bits, extra bytes, and payloads on ACKs. */
void test_packet_decoder_rejects_invalid_header_and_size(void)
{
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE + 1] = {0};
    size_t datagram_length = 0;

    packet.type = PACKET_ACK;
    packet.seq = 5;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_EQUAL_INT(PACKET_ACK, datagram[0]);
    TEST_ASSERT_TRUE(packet_decode(datagram, datagram_length, &decoded));
    TEST_ASSERT_EQUAL_UINT64(5, decoded.seq);

    datagram[1] = 1;
    TEST_ASSERT_EQUAL_INT(0, packet_decode(datagram, datagram_length, &decoded));
    datagram[1] = 0;
    TEST_ASSERT_EQUAL_INT(0, packet_decode(datagram, datagram_length + 1, &decoded));
    packet.payload_length = 1;
    TEST_ASSERT_EQUAL_INT(0, packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
}

/* Verify short datagrams, length mismatches, and unknown packet types are rejected. */
void test_packet_decoder_rejects_short_length_mismatch_and_unknown_type(void)
{
    packet_t packet = {0};
    packet_t decoded;
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE] = {0};
    size_t datagram_length = 0;

    packet.type = PACKET_ACK;
    packet.seq = 9;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_FALSE(packet_decode(datagram, PACKET_HEADER_SIZE - 1, &decoded));

    datagram[9] = 1;
    TEST_ASSERT_FALSE(packet_decode(datagram, datagram_length, &decoded));
    datagram[9] = 0;
    datagram[0] = 3;
    TEST_ASSERT_FALSE(packet_decode(datagram, datagram_length, &decoded));
}

/* Verify the receiver delivers only the expected sequence and ACKs duplicates/gaps. */
void test_receiver_acknowledges_only_in_order_data(void)
{
    receiver_state_t state;
    receiver_action_t action;
    packet_t packet = {0};

    receiver_state_init(&state, 100);
    packet.type = PACKET_DATA;
    packet.seq = 2;
    packet.payload_length = 1;
    packet.payload[0] = 'C';
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 200, &action));
    TEST_ASSERT_TRUE(action.send_ack);
    TEST_ASSERT_EQUAL_UINT64(0, action.ack_sequence);
    TEST_ASSERT_FALSE(action.deliver_payload);
    TEST_ASSERT_EQUAL_UINT64(0, state.expected);

    packet.seq = 0;
    packet.payload[0] = 'A';
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 300, &action));
    TEST_ASSERT_TRUE(action.deliver_payload);
    TEST_ASSERT_EQUAL_INT('A', action.payload[0]);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);
    TEST_ASSERT_EQUAL_UINT64(1, state.expected);

    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 400, &action));
    TEST_ASSERT_FALSE(action.deliver_payload);
    TEST_ASSERT_TRUE(action.send_ack);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);

    packet.seq = 4;
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&state, &packet, 500, &action));
    TEST_ASSERT_FALSE(action.deliver_payload);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);
}

/* Verify FIN is ACKed and duplicate FINs are handled during the linger period. */
void test_receiver_fin_acknowledges_and_lingers_until_deadline(void)
{
    receiver_state_t state;
    receiver_action_t action;
    packet_t packet = {0};

    receiver_state_init(&state, 100);
    packet.type = PACKET_FIN;
    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&state, &packet, 500, &action));
    TEST_ASSERT_TRUE(action.send_ack);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);
    TEST_ASSERT_EQUAL_UINT64(2500, action.timer_due_ms);

    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&state, &packet, 1000, &action));
    TEST_ASSERT_TRUE(action.send_ack);
    TEST_ASSERT_EQUAL_UINT64(1, action.ack_sequence);
    packet.seq = 9;
    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&state, &packet, 1100, &action));
    TEST_ASSERT_FALSE(action.send_ack);
    packet.type = PACKET_DATA;
    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&state, &packet, 1200, &action));
    TEST_ASSERT_FALSE(action.send_ack);
    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_timeout(&state, 2499, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_DONE,
                          receiver_on_timeout(&state, 2500, &action));
    TEST_ASSERT_FALSE(action.timer_armed);
}

/* Verify idle timeout calculations use the supplied clock value. */
void test_receiver_idle_timeout_uses_passed_time(void)
{
    receiver_state_t state;
    receiver_action_t action;

    receiver_state_init(&state, 100);
    TEST_ASSERT_EQUAL_UINT64(100 + RECEIVER_IDLE_TIMEOUT_MS,
                             receiver_next_deadline(&state));
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_timeout(&state,
                              100 + RECEIVER_IDLE_TIMEOUT_MS - 1, &action));
    TEST_ASSERT_EQUAL_INT(RECEIVER_TIMED_OUT,
                          receiver_on_timeout(&state,
                              100 + RECEIVER_IDLE_TIMEOUT_MS, &action));
}

/* Verify window capacity, cumulative ACK advancement, and duplicate ACK handling. */
void test_sender_window_full_cumulative_ack_and_duplicate_ack(void)
{
    sender_state_t sender;
    sender_action_t action;
    const unsigned char payload = 'x';
    uint32_t sequence;

    TEST_ASSERT_FALSE(sender_state_init(&sender, 0, 10));
    TEST_ASSERT_FALSE(sender_state_init(&sender, SENDER_MAX_WINDOW + 1, 10));
    TEST_ASSERT_FALSE(sender_state_init(&sender, 1, 0));
    TEST_ASSERT_TRUE(sender_state_init(&sender, 2, 10));
    TEST_ASSERT_TRUE(sender_can_accept_data(&sender));
    for (sequence = 0; sequence < 2; sequence++) {
        TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                              sender_on_data(&sender, &payload, 1, 0, &action));
    }
    TEST_ASSERT_FALSE(sender_can_accept_data(&sender));
    TEST_ASSERT_EQUAL_INT(SENDER_ERROR,
                          sender_on_data(&sender, &payload, 1, 0, &action));

    TEST_ASSERT_TRUE(sender_state_init(&sender, 4, 10));
    for (sequence = 0; sequence < 4; sequence++) {
        TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                              sender_on_data(&sender, &payload, 1, 0, &action));
    }
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&sender, 0, 4, &action));
    TEST_ASSERT_EQUAL_UINT64(0, sender.base);
    TEST_ASSERT_EQUAL_UINT64(10, sender.timer_due_ms);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&sender, 3, 5, &action));
    TEST_ASSERT_EQUAL_UINT64(3, sender.base);
    TEST_ASSERT_EQUAL_UINT64(4, sender.next);
    TEST_ASSERT_EQUAL_UINT64(15, sender.timer_due_ms);
    TEST_ASSERT_TRUE(sender_can_accept_data(&sender));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_ack(&sender, 3, 8, &action));
    TEST_ASSERT_EQUAL_UINT64(3, sender.base);
    TEST_ASSERT_EQUAL_UINT64(15, sender.timer_due_ms);
}

/* Verify a timeout retransmits every packet that remains outstanding. */
void test_sender_timeout_resends_every_packet_in_the_window(void)
{
    sender_state_t sender;
    sender_action_t action;
    const unsigned char payload = 'z';
    uint32_t sequence;

    TEST_ASSERT_TRUE(sender_state_init(&sender, 3, 10));
    for (sequence = 0; sequence < 3; sequence++) {
        TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                              sender_on_data(&sender, &payload, 1, 0, &action));
    }
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_timeout(&sender, 9, &action));
    TEST_ASSERT_EQUAL_UINT64(0, action.send_count);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_timeout(&sender, 10, &action));
    TEST_ASSERT_EQUAL_UINT64(3, action.send_count);
    for (sequence = 0; sequence < 3; sequence++) {
        TEST_ASSERT_EQUAL_UINT64(sequence, action.packets[sequence].seq);
        TEST_ASSERT_EQUAL_INT(PACKET_DATA, action.packets[sequence].type);
    }
}

/* Verify retransmission recovers a dropped DATA packet and completes the transfer. */
void test_sender_retransmits_a_lost_packet_and_completes_in_memory(void)
{
    sender_state_t sender;
    receiver_state_t receiver;
    sender_action_t sender_action;
    receiver_action_t receiver_action;
    packet_t second_packet;
    packet_t retransmitted_second;
    unsigned char delivered[2] = {0};
    size_t delivered_length = 0;
    unsigned char first = 'A';
    unsigned char second = 'B';

    TEST_ASSERT_TRUE(sender_state_init(&sender, 2, 10));
    receiver_state_init(&receiver, 0);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&sender, &first, 1, 0, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(1, sender_action.send_count);
    TEST_ASSERT_EQUAL_UINT64(0, sender_action.packets[0].seq);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&sender, &second, 1, 0, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(1, sender_action.packets[0].seq);
    second_packet = sender_action.packets[0];
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_eof(&sender, 0, &sender_action));

    /* Drop sequence zero; passing sequence one produces a duplicate ACK. */
    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&receiver, &second_packet, 1, &receiver_action));
    TEST_ASSERT_EQUAL_UINT64(0, receiver_action.ack_sequence);
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_ack(&sender, receiver_action.ack_sequence,
                                        1, &sender_action));

    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_timeout(&sender, 10, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(2, sender_action.send_count);
    TEST_ASSERT_EQUAL_UINT64(0, sender_action.packets[0].seq);
    TEST_ASSERT_EQUAL_UINT64(1, sender_action.packets[1].seq);
    retransmitted_second = sender_action.packets[1];

    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&receiver, &sender_action.packets[0],
                                             10, &receiver_action));
    TEST_ASSERT_TRUE(receiver_action.deliver_payload);
    delivered[delivered_length++] = receiver_action.payload[0];
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_ack(&sender, receiver_action.ack_sequence,
                                        10, &sender_action));

    TEST_ASSERT_EQUAL_INT(RECEIVER_CONTINUE,
                          receiver_on_packet(&receiver, &retransmitted_second,
                                             10, &receiver_action));
    TEST_ASSERT_TRUE(receiver_action.deliver_payload);
    delivered[delivered_length++] = receiver_action.payload[0];
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_ack(&sender, receiver_action.ack_sequence,
                                        10, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(1, sender_action.send_count);
    TEST_ASSERT_EQUAL_INT(PACKET_FIN, sender_action.packets[0].type);
    TEST_ASSERT_EQUAL_UINT64(2, sender_action.packets[0].seq);

    TEST_ASSERT_EQUAL_INT(RECEIVER_LINGERING,
                          receiver_on_packet(&receiver, &sender_action.packets[0],
                                             11, &receiver_action));
    TEST_ASSERT_EQUAL_INT(SENDER_DONE,
                          sender_on_ack(&sender, receiver_action.ack_sequence,
                                        11, &sender_action));
    TEST_ASSERT_EQUAL_UINT64(2, delivered_length);
    TEST_ASSERT_EQUAL_INT('A', delivered[0]);
    TEST_ASSERT_EQUAL_INT('B', delivered[1]);
}

/* Verify the sender reports failure after the configured number of timeouts. */
void test_sender_stops_after_ten_expired_timers(void)
{
    sender_state_t sender;
    sender_action_t action;
    const unsigned char payload = 'x';
    uint32_t attempt;

    TEST_ASSERT_TRUE(sender_state_init(&sender, 1, 10));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&sender, &payload, 1, 0, &action));
    for (attempt = 1; attempt <= SENDER_MAX_TIMEOUTS; attempt++) {
        sender_status_t expected = attempt == SENDER_MAX_TIMEOUTS
            ? SENDER_ERROR : SENDER_ACTIVE;
        TEST_ASSERT_EQUAL_INT(expected,
                              sender_on_timeout(&sender, attempt * 10, &action));
        if (attempt < SENDER_MAX_TIMEOUTS) {
            TEST_ASSERT_EQUAL_UINT64(1, action.send_count);
        }
    }
}

/* Verify a final FIN can use the sequence following the maximum DATA packet count. */
void test_sender_allows_eof_after_maximum_data_packet_count(void)
{
    sender_state_t sender;
    sender_action_t action;

    TEST_ASSERT_TRUE(sender_state_init(&sender, 1, 10));
    sender.base = SENDER_MAX_DATA_PACKETS;
    sender.next = SENDER_MAX_DATA_PACKETS;
    TEST_ASSERT_TRUE(sender_can_accept_data(&sender));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE, sender_on_eof(&sender, 100, &action));
    TEST_ASSERT_EQUAL_UINT64(1, action.send_count);
    TEST_ASSERT_EQUAL_INT(PACKET_FIN, action.packets[0].type);
    TEST_ASSERT_EQUAL_UINT64(SENDER_MAX_DATA_PACKETS, action.packets[0].seq);
}

/* Verify an empty input completes successfully with no delivered DATA packets. */
void test_empty_file_transfers_without_data_packets(void)
{
    unsigned char delivered[1];
    size_t delivered_length = 99;
    size_t delivered_packets = 99;

    TEST_ASSERT_TRUE(test_simulate_bytes(NULL, 0, UINT64_C(41), 0,
                                         delivered, sizeof(delivered),
                                         &delivered_length, &delivered_packets));
    TEST_ASSERT_EQUAL_UINT64(0, delivered_length);
    TEST_ASSERT_EQUAL_UINT64(0, delivered_packets);
}

/* Verify exact payload multiples do not produce an extra empty DATA packet. */
void test_exact_multiple_of_packet_payload_transfers_without_empty_data(void)
{
    unsigned char input[2 * PACKET_MAX_PAYLOAD];
    unsigned char delivered[sizeof(input)];
    size_t delivered_length;
    size_t delivered_packets;
    size_t index;

    for (index = 0; index < sizeof(input); index++) {
        input[index] = (unsigned char)(index * 17U);
    }
    TEST_ASSERT_TRUE(test_simulate_bytes(input, sizeof(input), UINT64_C(53), 0,
                                         delivered, sizeof(delivered),
                                         &delivered_length, &delivered_packets));
    TEST_ASSERT_EQUAL_UINT64(sizeof(input), delivered_length);
    TEST_ASSERT_EQUAL_UINT64(2, delivered_packets);
    TEST_ASSERT_TRUE(memcmp(input, delivered, sizeof(input)) == 0);
}

/* Verify repeatable loss, corruption, and duplication in both directions are recovered. */
void test_complete_transfer_survives_deterministic_bidirectional_impairment(void)
{
    unsigned char input[16384];
    unsigned char delivered[sizeof(input)];
    const uint64_t seeds[] = {UINT64_C(7), UINT64_C(107), UINT64_C(525)};
    size_t delivered_length;
    size_t delivered_packets;
    size_t index;
    size_t seed_index;

    for (index = 0; index < sizeof(input); index++) {
        input[index] = (unsigned char)((index * 37U + index / 7U) & 0xffU);
    }
    for (seed_index = 0; seed_index < sizeof(seeds) / sizeof(seeds[0]); seed_index++) {
        TEST_ASSERT_TRUE(test_simulate_bytes(input, sizeof(input), seeds[seed_index], 1,
                                             delivered, sizeof(delivered),
                                             &delivered_length, &delivered_packets));
        TEST_ASSERT_EQUAL_UINT64(sizeof(input), delivered_length);
        TEST_ASSERT_TRUE(memcmp(input, delivered, sizeof(input)) == 0);
    }
}

/* Verify public sender, receiver, and relay APIs handle invalid arguments safely. */
void test_network_api_rejects_invalid_arguments(void)
{
    relay_client_t client = {-1, {0}, 0, 0};
    unsigned char datagram[PACKET_HEADER_SIZE] = {0};
    packet_t packet;

    TEST_ASSERT_EQUAL_INT(1, sender_send_file(NULL, "relay", 9000, "input", 1, 1,
                                               0.0, 0.0, 0.0));
    TEST_ASSERT_EQUAL_INT(1, receiver_receive_file(NULL, "relay", 9000, "output"));
    TEST_ASSERT_EQUAL_INT(2, sender_send_file("session", NULL, 9000, "input", 1, 1,
                                               0.0, 0.0, 0.0));
    TEST_ASSERT_EQUAL_INT(2, sender_send_file("session", "relay", 9000, NULL, 1, 1,
                                               0.0, 0.0, 0.0));
    TEST_ASSERT_EQUAL_INT(2, receiver_receive_file("session", NULL, 9000, "output"));
    TEST_ASSERT_EQUAL_INT(2, receiver_receive_file("session", "relay", 9000, NULL));
    TEST_ASSERT_FALSE(relay_client_open(NULL, "localhost", 9000));
    TEST_ASSERT_FALSE(relay_client_open(&client, NULL, 9000));
    relay_client_close(NULL);
    relay_client_close(&client);
    TEST_ASSERT_FALSE(relay_client_register_receiver(NULL, "session"));
    TEST_ASSERT_FALSE(relay_client_register_receiver(&client, "session"));
    client.socket_fd = 1;
    TEST_ASSERT_FALSE(relay_client_register_receiver(&client, "bad_name"));
    client.socket_fd = -1;
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.0, 0.0));
    client.socket_fd = 1;
    TEST_ASSERT_FALSE(relay_client_register_sender(NULL, "session", 0.0, 0.0, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", NAN, 0.0, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, NAN, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.0, NAN));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", -0.1, 0.0, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.6, 0.0, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, -0.1, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.6, 0.0));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.0, -0.1));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, 0.0, 0.6));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "session", 0.0, INFINITY, 0.0));
    TEST_ASSERT_EQUAL_INT(-1,
                          relay_client_send_datagram(NULL, datagram, sizeof(datagram)));
    TEST_ASSERT_EQUAL_INT(RELAY_IO_ERROR,
                          relay_client_receive_packet(NULL, 0, &packet));
    TEST_ASSERT_FALSE(relay_client_register_sender(&client, "bad_name", 0.0, 0.0, 0.0));
    TEST_ASSERT_EQUAL_INT(-1, relay_client_send_datagram(&client, NULL, 0));
    client.clock_failed = 1;
    TEST_ASSERT_FALSE(relay_client_clock_ok(&client));
    TEST_ASSERT_EQUAL_INT(RELAY_IO_ERROR,
                          relay_client_receive_packet(&client, 0, &packet));
    client.socket_fd = -1;
    client.clock_failed = 0;
    TEST_ASSERT_EQUAL_INT(-1, relay_client_send_datagram(&client, datagram, sizeof(datagram)));
    TEST_ASSERT_EQUAL_INT(RELAY_IO_ERROR,
                          relay_client_receive_packet(&client, 0, &packet));
    TEST_ASSERT_TRUE(relay_client_clock_ok(&client));
    (void)relay_client_now(&client);
    TEST_ASSERT_TRUE(relay_client_clock_ok(&client));
    TEST_ASSERT_EQUAL_UINT64(0, relay_client_now(NULL));
    TEST_ASSERT_FALSE(relay_client_clock_ok(NULL));
}

/* Verify remaining protocol edge cases and overflow guards are covered. */
void test_protocol_guard_and_overflow_paths(void)
{
    sender_state_t sender;
    sender_action_t action;
    unsigned char data = 'x';
    packet_t packet = {0};
    packet_t decoded = {0};
    unsigned char datagram[PACKET_MAX_DATAGRAM_SIZE];
    size_t datagram_length = 0;

    TEST_ASSERT_TRUE(sender_state_init(&sender, 1, 1));
    TEST_ASSERT_EQUAL_INT(SENDER_ACTIVE,
                          sender_on_data(&sender, &data, 1, UINT64_MAX, &action));
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, sender.timer_due_ms);
    TEST_ASSERT_EQUAL_UINT64(UINT64_MAX, action.timer_due_ms);

    packet.type = PACKET_FIN;
    packet.seq = 42U;
    TEST_ASSERT_TRUE(packet_encode(&packet, datagram, sizeof(datagram), &datagram_length));
    TEST_ASSERT_EQUAL_INT(PACKET_FIN, datagram[0]);
    TEST_ASSERT_TRUE(packet_decode(datagram, datagram_length, &decoded));
    TEST_ASSERT_EQUAL_INT(PACKET_FIN, decoded.type);
    TEST_ASSERT_EQUAL_UINT64(42U, decoded.seq);
}

/* Verify a zero-timeout read still receives a packet that is already queued. */
void test_relay_client_zero_timeout_checks_queued_packet(void)
{
    int sockets[2] = {-1, -1};
    relay_client_t client = {-1, {0}, 0, 0};
    packet_t outgoing = {0};
    packet_t incoming = {0};
    unsigned char datagram[PACKET_HEADER_SIZE];
    size_t datagram_length = 0;
    int encode_ok;
    ssize_t sent = -1;
    relay_io_result_t packet_status = RELAY_IO_ERROR;
    relay_io_result_t empty_status = RELAY_IO_ERROR;

    TEST_ASSERT_EQUAL_INT(0, socketpair(AF_UNIX, SOCK_DGRAM, 0, sockets));
    client.socket_fd = sockets[0];
    outgoing.type = PACKET_ACK;
    outgoing.seq = 3;
    encode_ok = packet_encode(&outgoing, datagram, sizeof(datagram), &datagram_length);
    if (encode_ok) {
        sent = send(sockets[1], datagram, datagram_length, 0);
        if (sent == (ssize_t)datagram_length) {
            packet_status = relay_client_receive_packet(&client, 0, &incoming);
            if (packet_status == RELAY_IO_PACKET) {
                empty_status = relay_client_receive_packet(&client, 0, &incoming);
            }
        }
    }
    (void)close(sockets[0]);
    (void)close(sockets[1]);
    client.socket_fd = -1;

    TEST_ASSERT_TRUE(encode_ok);
    TEST_ASSERT_EQUAL_INT(datagram_length, sent);
    TEST_ASSERT_EQUAL_INT(RELAY_IO_PACKET, packet_status);
    TEST_ASSERT_EQUAL_INT(PACKET_ACK, incoming.type);
    TEST_ASSERT_EQUAL_UINT64(3, incoming.seq);
    TEST_ASSERT_EQUAL_INT(RELAY_IO_TIMEOUT, empty_status);
}

/* Verify rejected/malformed registration replies and retry exhaustion are handled. */
void test_relay_registration_rejects_bad_replies_and_retries(void)
{
    TEST_ASSERT_TRUE(test_registration_result("ERR denied", 1, 0, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result("NO", 1, 0, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result("FAIL", 1, 0, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result(NULL, 5, 0, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result("OK_PACKET", 1, 1, 0, 1));
    TEST_ASSERT_TRUE(test_registration_result("ERR denied", 1, 2, 1, 1));
    TEST_ASSERT_TRUE(test_registration_result("ERR denied", 1, 2, 2, 1));
    TEST_ASSERT_TRUE(test_registration_result("OK", 1, 2, 1, 0));
}

/* Verify network receiver timeout and sequence-overflow errors are reported. */
void test_receiver_network_timeout_and_state_error_paths(void)
{
    int timed_out;
    int state_error;

    receiver_io_test_set_mode(RECEIVER_IO_TEST_EXPIRED);
    timed_out = test_registration_result("OK", 1, 2, 2, 1);
    receiver_io_test_set_mode(RECEIVER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(timed_out);

    receiver_io_test_set_mode(RECEIVER_IO_TEST_SEQUENCE_OVERFLOW);
    state_error = test_registration_result("OK_DATA_MAX", 1, 2, 2, 1);
    receiver_io_test_set_mode(RECEIVER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(state_error);
}

/* Verify the network sender fails when the relay repeatedly drops its DATA packets. */
void test_sender_network_reports_repeated_timeout_failure(void)
{
    char input_path[] = "/tmp/cs525-timeout-XXXXXX";
    struct sockaddr_in relay_address = {0};
    socklen_t relay_address_length = sizeof(relay_address);
    int relay_fd = socket(AF_INET, SOCK_DGRAM, 0);
    int input_fd;
    pid_t relay_pid;
    int relay_status;
    int sender_status;
    const unsigned char payload = 'x';

    TEST_ASSERT_TRUE(relay_fd >= 0);
    relay_address.sin_family = AF_INET;
    relay_address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    TEST_ASSERT_EQUAL_INT(0, bind(relay_fd, (struct sockaddr *)&relay_address,
                                  sizeof(relay_address)));
    TEST_ASSERT_EQUAL_INT(0, getsockname(relay_fd, (struct sockaddr *)&relay_address,
                                         &relay_address_length));
    input_fd = mkstemp(input_path);
    TEST_ASSERT_TRUE(input_fd >= 0);
    TEST_ASSERT_EQUAL_INT(sizeof(payload), write(input_fd, &payload, sizeof(payload)));
    TEST_ASSERT_EQUAL_INT(0, close(input_fd));

    relay_pid = fork();
    TEST_ASSERT_TRUE(relay_pid >= 0);
    if (relay_pid == 0) {
        int status;
        __gcov_reset();
        status = test_drop_ack_server(relay_fd);
        __gcov_dump();
        _exit(status);
    }
    TEST_ASSERT_EQUAL_INT(0, close(relay_fd));
    sender_status = sender_send_file("coveragetest", "127.0.0.1",
                                     ntohs(relay_address.sin_port), input_path,
                                     1, 10, 0.0, 0.0, 0.0);
    TEST_ASSERT_EQUAL_INT(relay_pid, waitpid(relay_pid, &relay_status, 0));
    TEST_ASSERT_EQUAL_INT(2, sender_status);
    TEST_ASSERT_TRUE(WIFEXITED(relay_status));
    TEST_ASSERT_EQUAL_INT(0, WEXITSTATUS(relay_status));
    TEST_ASSERT_EQUAL_INT(0, unlink(input_path));
}

/* Verify network sender guards reject a missing timer and malformed ACK payload. */
void test_sender_network_state_guards(void)
{
    int inactive_timer;
    int nonempty_ack;
    int data_limit;
    int fin_sequence_limit;

    TEST_ASSERT_TRUE(sender_io_test_rejects_invalid_action());
    TEST_ASSERT_TRUE(receiver_io_test_rejects_invalid_packet());
    sender_io_test_set_mode(SENDER_IO_TEST_DISARM_TIMER_AFTER_FIN);
    inactive_timer = test_registration_result("OK", 1, 2, 1, 1);
    sender_io_test_set_mode(SENDER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(inactive_timer);

    sender_io_test_set_mode(SENDER_IO_TEST_NONEMPTY_ACK);
    nonempty_ack = test_registration_result("OK_PACKET", 1, 2, 1, 1);
    sender_io_test_set_mode(SENDER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(nonempty_ack);

    sender_io_test_set_mode(SENDER_IO_TEST_DATA_LIMIT);
    data_limit = test_registration_result("OK", 1, 2, 1, 1);
    sender_io_test_set_mode(SENDER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(data_limit);

    sender_io_test_set_mode(SENDER_IO_TEST_FIN_SEQUENCE_LIMIT);
    fin_sequence_limit = test_registration_result("OK", 1, 2, 1, 1);
    sender_io_test_set_mode(SENDER_IO_TEST_NORMAL);
    TEST_ASSERT_TRUE(fin_sequence_limit);
}

/* Register and execute the project's Unity unit and integration tests. */
int main(void)
{
    UNITY_BEGIN();
    RUN_TEST(test_session_name_validation);
    RUN_TEST(test_checksum_matches_rfc1071_example);
    RUN_TEST(test_checksum_handles_odd_length_and_detects_a_flipped_bit);
    RUN_TEST(test_packet_wire_format_round_trips_and_checks_checksum);
    RUN_TEST(test_packet_decoder_rejects_invalid_header_and_size);
    RUN_TEST(test_packet_decoder_rejects_short_length_mismatch_and_unknown_type);
    RUN_TEST(test_packet_api_rejects_invalid_arguments);
    RUN_TEST(test_receiver_acknowledges_only_in_order_data);
    RUN_TEST(test_receiver_fin_acknowledges_and_lingers_until_deadline);
    RUN_TEST(test_receiver_idle_timeout_uses_passed_time);
    RUN_TEST(test_receiver_state_rejects_invalid_transitions_and_saturates);
    RUN_TEST(test_sender_window_full_cumulative_ack_and_duplicate_ack);
    RUN_TEST(test_sender_timeout_resends_every_packet_in_the_window);
    RUN_TEST(test_sender_retransmits_a_lost_packet_and_completes_in_memory);
    RUN_TEST(test_sender_stops_after_ten_expired_timers);
    RUN_TEST(test_sender_allows_eof_after_maximum_data_packet_count);
    RUN_TEST(test_sender_state_rejects_invalid_transitions_and_saturates);
    RUN_TEST(test_empty_file_transfers_without_data_packets);
    RUN_TEST(test_exact_multiple_of_packet_payload_transfers_without_empty_data);
    RUN_TEST(test_complete_transfer_survives_deterministic_bidirectional_impairment);
    RUN_TEST(test_network_api_rejects_invalid_arguments);
    RUN_TEST(test_protocol_guard_and_overflow_paths);
    RUN_TEST(test_relay_client_zero_timeout_checks_queued_packet);
    RUN_TEST(test_relay_registration_rejects_bad_replies_and_retries);
    RUN_TEST(test_receiver_network_timeout_and_state_error_paths);
    RUN_TEST(test_sender_network_reports_repeated_timeout_failure);
    RUN_TEST(test_sender_network_state_guards);
    RUN_TEST(test_network_transfer_through_loopback_relay);
    RUN_TEST(test_empty_network_transfer_sends_only_fin);
    RUN_TEST(test_exact_payload_network_transfer);
    return UNITY_END();
}
```

## Scripts Files
Report generated on 10/06/2026 at 15:42:03


---

## End of Report

SHA-256 Hash of the report: da11c55f5b91dbe0acf6a04070b13c584651372b4f077384b5778b2dc8b0eabe

Do not edit the generated report. Any changes will be reported as academic dishonesty

---
## GitHub Info
- GitHub repo name: lmatthews321/cs525-p2
- The repository visibility is public.
- The workflow was triggered by lmatthews321
