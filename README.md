# CS118 Project 2

This is the repo for spring 2022 cs118 project 2.

Danny Lee seunghoonlee95@g.ucla.edu 105128998
Kyle Wong kyleykkw@g.ucla.edu 405103963

## Makefile

This provides a couple make targets for things.
By default (all target), it makes the `server` and `client` executables.

It provides a `clean` target, and `tarball` target to create the submission file as well.

You will need to modify the `Makefile` USERID to add your userid for the `.tar.gz` turn-in at the top of the file.

## Academic Integrity Note

You are encouraged to host your code in private repositories on [GitHub](https://github.com/), [GitLab](https://gitlab.com), or other places.  At the same time, you are PROHIBITED to make your code for the class project public during the class or any time after the class.  If you do so, you will be violating academic honestly policy that you have signed, as well as the student code of conduct and be subject to serious sanctions.

## High Level Design of the server and client.

Based on the skeleton code that was provided, our group added features of transmitting files between the server and client. 
After the 3-way handshake is done and the first data packet is sent by the client, the server sends back an ACK with a specified
sequence number and ACK number. When sending a large file, the client splits the data into packets and transmits to the server.
We built and kept track of the packets that were sent to the server but have not yet been acknowledged via window array. The window is an array of WND_SIZE that holds packets in them. They keep the packets in the window in case there is a packet loss and we need 
to retransmit the packets. 

## The problems you ran into and how you solved the problems.

Our team had difficulties solving the loss handling. The issue we had was that the server writes the data to an output file (i.e 1.file) but it writes more than it is supposed to write. That is, if the input file reading in was 3KB, the output file we get as a result was like 5KB. The cause of the issue was because of the data packages that came right after the dupACK package. Although the server should dismiss the dupACK and all the following packages within the same window, it only dismissed the first package that was a duplicate and wrote the rest on to the output file. In order to fix this, we implemented an array 'receiver_window' to keep track of which packets the server have already received. If the newly incoming packet is already within the receiver_window, then we detect there was an ACK loss from server -> client and we don't write to the output file. 


Another issue came when attempting to implement Selective Repeat protocol for loss handling. The code works as expected with no data or ACK loss. On the client side, our implementation seems to get stuck in an infinite loop, or just completely stop running after a while. We tried to use the circular buffer data structure to handle the sender and receiver windows. However, there seems to be issues in our break conditions in the inner loops when iterating through this buffer. Our main break condition for the client code is when there is no more data to send from the requested file, and there are no more packets left in our sender window. This break condition works fine, confirmed through tests and print statements. However, we suspect one of the inner loops which typically work with the start and end (s and e) indices of the sender window, gets stuck in an infinite loop. And this issue only occurs when dealing with loss. We also know it is an issue on the client end because the output shows that the client received an ACK packet before the program freezes.









