#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <unistd.h>
#include <sys/time.h>
#include <fcntl.h>

// =====================================

#define RTO 500000 /* timeout in microseconds */
#define HDR_SIZE 12 /* header size*/
#define PKT_SIZE 524 /* total packet size */
#define PAYLOAD_SIZE 512 /* PKT_SIZE - HDR_SIZE */
#define WND_SIZE 10 /* window size*/
#define MAX_SEQN 25601 /* number of sequence numbers [0-25600] */

// Packet Structure: Described in Section 2.1.1 of the spec. DO NOT CHANGE!
struct packet {
    unsigned short seqnum;
    unsigned short acknum;
    char syn;
    char fin;
    char ack;
    char dupack;
    unsigned int length;
    char payload[PAYLOAD_SIZE];
};

// Printing Functions: Call them on receiving/sending/packet timeout according
// Section 2.6 of the spec. The content is already conformant with the spec,
// no need to change. Only call them at correct times.
void printRecv(struct packet* pkt) {
    printf("RECV %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", (pkt->ack || pkt->dupack) ? " ACK": "");
}

void printSend(struct packet* pkt, int resend) {
    if (resend)
        printf("RESEND %d %d%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "");
    else
        printf("SEND %d %d%s%s%s%s\n", pkt->seqnum, pkt->acknum, pkt->syn ? " SYN": "", pkt->fin ? " FIN": "", pkt->ack ? " ACK": "", pkt->dupack ? " DUP-ACK": "");
}

void printTimeout(struct packet* pkt) {
    printf("TIMEOUT %d\n", pkt->seqnum);
}

// Building a packet by filling the header and contents.
// This function is provided to you and you can use it directly
void buildPkt(struct packet* pkt, unsigned short seqnum, unsigned short acknum, char syn, char fin, char ack, char dupack, unsigned int length, const char* payload) {
    pkt->seqnum = seqnum;
    pkt->acknum = acknum;
    pkt->syn = syn;
    pkt->fin = fin;
    pkt->ack = ack;
    pkt->dupack = dupack;
    pkt->length = length;
    memcpy(pkt->payload, payload, length);
}

// =====================================

double setTimer() {
    struct timeval e;
    gettimeofday(&e, NULL);
    return (double) e.tv_sec + (double) e.tv_usec/1000000 + (double) RTO/1000000;
}

int isTimeout(double end) {
    struct timeval s;
    gettimeofday(&s, NULL);
    double start = (double) s.tv_sec + (double) s.tv_usec/1000000;
    return ((end - start) < 0.0);
}

// =====================================

int main (int argc, char *argv[])
{
    if (argc != 2) {
        perror("ERROR: incorrect number of arguments\n");
        exit(1);
    }

    unsigned int servPort = atoi(argv[1]);

    // =====================================
    // Socket Setup

    int sockfd;
    struct sockaddr_in servaddr, cliaddr;

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);

    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(servPort);
    memset(servaddr.sin_zero, '\0', sizeof(servaddr.sin_zero));

    if (bind(sockfd, (struct sockaddr *) &servaddr, sizeof(servaddr)) == -1) {
        perror("bind() error");
        exit(1);
    }

    int cliaddrlen = sizeof(cliaddr);

    // NOTE: We set the socket as non-blocking so that we can poll it until
    //       timeout instead of getting stuck. This way is not particularly
    //       efficient in real programs but considered acceptable in this
    //       project.
    //       Optionally, you could also consider adding a timeout to the socket
    //       using setsockopt with SO_RCVTIMEO instead.
    fcntl(sockfd, F_SETFL, O_NONBLOCK);

    // =====================================

    unsigned short seqNum = (rand() * rand()) % MAX_SEQN;

    for (int i = 1; ; i++) {
        // =====================================
        // Establish Connection: This procedure is provided to you directly and
        // is already working.

        int n;

        FILE* fp;

        struct packet synpkt, synackpkt, ackpkt;

        while (1) {
            n = recvfrom(sockfd, &synpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
            if (n > 0) {
                printRecv(&synpkt);
                if (synpkt.syn)
                    break;
            }
        }

        unsigned short cliSeqNum = (synpkt.seqnum + 1) % MAX_SEQN; // next message from client should have this sequence number(ACK number sent from server)

        buildPkt(&synackpkt, seqNum, cliSeqNum, 1, 0, 1, 0, 0, NULL);   //send SYN-ACK packet back to client.

        while (1) {
            printSend(&synackpkt, 0);
            sendto(sockfd, &synackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
            
            while(1) {
                n = recvfrom(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
                if (n > 0) {
                    printRecv(&ackpkt);
                    if (ackpkt.seqnum == cliSeqNum && ackpkt.ack && ackpkt.acknum == (synackpkt.seqnum + 1) % MAX_SEQN) {

                        int length = snprintf(NULL, 0, "%d", i) + 6;
                        char* filename = malloc(length);
                        snprintf(filename, length, "%d.file", i);

                        fp = fopen(filename, "w");
                        free(filename);
                        if (fp == NULL) {
                            perror("ERROR: File could not be created\n");
                            exit(1);
                        }

                        fwrite(ackpkt.payload, 1, ackpkt.length, fp);

                        seqNum = ackpkt.acknum;
                        cliSeqNum = (ackpkt.seqnum + ackpkt.length) % MAX_SEQN;

                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                        printSend(&ackpkt, 0);
                        sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);

                        break;
                    }
                    else if (ackpkt.syn) {
                        buildPkt(&synackpkt, seqNum, (synpkt.seqnum + 1) % MAX_SEQN, 1, 0, 0, 1, 0, NULL);
                        break;
                    }
                }
            }

            if (! ackpkt.syn)
                break;
        }





        // *********************************************************************************************************************************
        // *********************************************************************************************************************************

        // *** TODO: Implement the rest of reliable transfer in the server ***
        // Implement GBN for basic requirement or Selective Repeat to receive bonus
        
        struct packet recvpkt;

        // receiver window
        struct packet receiver_window[WND_SIZE];
        int s = 0;  // window start
        int e = 0;  // window end
        int full = 0;

        while(1) {
            int expected_seqnum = cliSeqNum;
            int ack_loss = 0;
            n = recvfrom(sockfd, &recvpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
            if (n > 0) {
                printRecv(&recvpkt);

                // check if packet already in receiver window 
                // if already received -> ACK loss(client retransmitted the packet.)
                for (int i = s; i != e; i = (i + 1) % WND_SIZE) {
                    if (receiver_window[i].seqnum == recvpkt.seqnum) {  //packet is already in the receiver window.
                        ack_loss = 1;
                    }
                }

                // if not already in receiver window (no ACK loss), add the received packet
                // if ACK_loss, nothing to do because the packet is already in the receiver_window.
                if (!ack_loss) {   

                    // case 1: receiver window is not full,
                    // add packet to window, increase 'e' position
                    if (!full) {
                        receiver_window[e] = recvpkt;
                        int next = (e + 1) % WND_SIZE;
                        if (next == s) {
                            full = 1;
                        }else {
                            e = (e + 1) % WND_SIZE;
                        }
                    }

                    // case 2: receiver window is full,
                    // slide entire window over, add packet to window
                    else {
                        s = (s + 1) % WND_SIZE;
                        e = (e + 1) % WND_SIZE;
                        receiver_window[e] = recvpkt;
                    }
                }

                // *** build response ***

                // case 1: no more data to be received
                if (recvpkt.fin) {
                    cliSeqNum = (recvpkt.seqnum + 1) % MAX_SEQN;

                    // if no ACK loss, respond with regular ACK,
                    // if ACK loss, respond with DUP ACK
                    if (!ack_loss){
                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                    }
                    else {
                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 0, 1, 0, NULL);
                    }

                    printSend(&ackpkt, 0);
                    sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);

                    // while loop break condition: received FIN and the sequence numbers match 
                    if (recvpkt.seqnum == expected_seqnum) {
                        break;
                    }

                    // data or ACK loss, continue waiting for expected_seqnum
                    // reset cliSeqNum so that expected_seqnum is correct next loop iteration
                    else {
                        cliSeqNum = expected_seqnum;
                    }
                }

                // case 2: still more data to be received
                else {
                    cliSeqNum = (recvpkt.seqnum + recvpkt.length) % MAX_SEQN;

                    // if no ACK loss, respond with regular ACK,
                    // if ACK loss, respond with DUP ACK
                    if (!ack_loss) {
                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 1, 0, 0, NULL);
                    }
                    else {
                        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 0, 1, 0, NULL);
                    }

                    printSend(&ackpkt, 0);
                    sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);

                    // if received expected packet, write to fd
                    if (recvpkt.seqnum == expected_seqnum) {
                        fwrite(recvpkt.payload, 1, recvpkt.length, fp);

                        // check if next packet(s) also already received (should be consecutive in the window), 
                        // update cliSeqNum to the next unreceived sequence number if they are
                        int got_next = 0;
                        int next_index;
                        for (int i = s; i != e; i = (i + 1) % WND_SIZE) {
                            if (receiver_window[i].seqnum == cliSeqNum) {
                                got_next = 1;
                                next_index = (i + 1) % WND_SIZE;
                                fwrite(receiver_window[i].payload, 1, receiver_window[i].length, fp);
                                cliSeqNum = (receiver_window[i].seqnum + receiver_window[i].length) % MAX_SEQN;
                                break;
                            }
                        }
                        while (got_next) {
                            if (receiver_window[next_index].seqnum == cliSeqNum) {
                                fwrite(receiver_window[next_index].payload, 1, receiver_window[next_index].length, fp);
                                cliSeqNum = (receiver_window[next_index].seqnum + receiver_window[next_index].length) % MAX_SEQN;
                                next_index = (next_index + 1) % WND_SIZE;
                            }
                            else {
                                got_next = 0;
                            }
                        }
                    }

                    // if received unexpected packet, 
                    // reset cliSeqNum so that expected_seqnum is correct next loop iteration
                    else if (recvpkt.seqnum != expected_seqnum) {
                        cliSeqNum = expected_seqnum;
                    }
                }
            }
        }

        // *** End of your server implementation ***
        
        // *********************************************************************************************************************************
        // *********************************************************************************************************************************





        fclose(fp);
        // =====================================
        // Connection Teardown: This procedure is provided to you directly and
        // is already working.

        struct packet finpkt, lastackpkt;
        buildPkt(&finpkt, seqNum, 0, 0, 1, 0, 0, 0, NULL);
        buildPkt(&ackpkt, seqNum, cliSeqNum, 0, 0, 0, 1, 0, NULL);

        printSend(&finpkt, 0);
        sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
        double timer = setTimer();

        while (1) {
            while (1) {
                n = recvfrom(sockfd, &lastackpkt, PKT_SIZE, 0, (struct sockaddr *) &cliaddr, (socklen_t *) &cliaddrlen);
                if (n > 0)
                    break;

                if (isTimeout(timer)) {
                    printTimeout(&finpkt);
                    printSend(&finpkt, 1);
                    sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                    timer = setTimer();
                }
            }

            printRecv(&lastackpkt);
            if (lastackpkt.fin) {

                printSend(&ackpkt, 0);
                sendto(sockfd, &ackpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);

                printSend(&finpkt, 1);
                sendto(sockfd, &finpkt, PKT_SIZE, 0, (struct sockaddr*) &cliaddr, cliaddrlen);
                timer = setTimer();
                
                continue;
            }
            if ((lastackpkt.ack || lastackpkt.dupack) && lastackpkt.acknum == (finpkt.seqnum + 1) % MAX_SEQN)
                break;
        }

        seqNum = lastackpkt.acknum;
    }
}