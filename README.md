# Project cs525-p2 Reliable Data Transfer

- Name: Lael Matthews
- Email: laelmatthews@u.boisestate.edu
- Class: CS525

## Known Bugs or Issues

There is a possibililty that the FIN recovery can fail if the timeout is excessively long. However with a receiver linger time of 2 seconds this is unlikely. 
There are several test exclusions for System type calls (37) File I/O calls (7), State-machine/ protocol invariants (4) and control flow annotations/ test only injections (5). 

## Experience

This was the first project that utilized the AI agent exclusively for all code production. It was magical how quickly a project of this magnitude could be generated. However, with all the generated code it's difficult to really understand everything that was generated. I can get the feel for each function, but fully understanding everything would take almost as long and trying to write all the code myself. It's like black box programing now. We say what inputs and outputs we want and just leave the internals to AI. I did enjoy testing out the finished project by sending and receiving through the relay and understand the concepts of the Go-Back-N protocol. 

## Analysis

The primary analysis for this project is how the window size affects the performance of our Go-Back-N protocol. A larger window reduces time and increases throughput because more packets can be sent at a time. Loss does affect larger window systems more. They are still faster, but suffer a higher percent of degradation as discussed in the results below. Another feature of the protocol that is tested is the checksum which validates that messages arrive uncorrupted. The program successfully computed the expected value in the lab test file and during actual data transfer through the relay. The three other main pieces of the protocol (sequence numbers, cumulative acknowledgements, and the sender timer) all help ensure packets are received in the correct order. This is proven to work by noting that the file received matches the one that was sent, even if loss, corruption and/or duplication are added. I attempted a few different combinations of these variables and all received files matching the original sent.


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