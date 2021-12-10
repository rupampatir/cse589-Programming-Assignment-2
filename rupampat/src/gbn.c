#include "../include/simulator.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ******************************************************************
 ALTERNATING BIT AND GO-BACK-N NETWORK EMULATOR: VERSION 1.1  J.F.Kurose

   This code should be used for PA2, unidirectional data transfer 
   protocols (from A to B). Network properties:
   - one way network delay averages five time units (longer if there
     are other messages in the channel for GBN), but can be larger
   - packets can be corrupted (either the header or the data portion)
     or lost, according to user-defined probabilities
   - packets will be delivered in the order in which they were sent
     (although some can be lost).
**********************************************************************/

/********* STUDENTS WRITE THE NEXT SEVEN ROUTINES *********/

#define A 0
#define B 1

int create_checksum(struct pkt packet);

struct buffer {
  struct msg message;
  struct buffer * next_message;
}* buffered_messages;

int seqnum_A;
int acknum_B;
int TIMEOUT;
int WINDOWSIZE;
int base;
int nextseqnum;

int create_checksum(struct pkt packet) {
  int checksum = 0;
  // if it's a ack
  if (packet.payload == NULL) {
    return checksum;
  }
  checksum += packet.seqnum;
  checksum += packet.acknum;
  for (int i = 0; i < 20; i++) {
    checksum += (unsigned char) packet.payload[i];
  }
  return checksum;
}

/* called from layer 5, passed the data to be sent to other side */
void A_output(message)
struct msg message; {
  struct buffer * temp = buffered_messages;
  if (temp != NULL) {
    while (temp -> next_message != NULL) {
      temp = temp -> next_message;
    }
    temp -> next_message = malloc(sizeof(struct buffer));
    if (temp -> next_message == NULL) {
      exit(0);
    }
    temp -> next_message -> message = message;
    temp -> next_message -> next_message = NULL;
  } else {
    buffered_messages = malloc(sizeof(struct buffer));
    if (buffered_messages == NULL) {
      exit(0);
    }
    buffered_messages -> message = message;
    buffered_messages -> next_message = NULL;
  }

  if (nextseqnum < base + WINDOWSIZE) {
    struct pkt next_packet;
    next_packet.seqnum = nextseqnum;
    next_packet.acknum = nextseqnum;
    memcpy(next_packet.payload, message.data, sizeof(message.data));
    next_packet.checksum = create_checksum(next_packet);
    tolayer3(A, next_packet);
    if (nextseqnum == base) {
      starttimer(A, TIMEOUT);
    }
  }
  nextseqnum++;
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
struct pkt packet; {
  // Ack received

  int checksum = create_checksum(packet);

  // if corrupted or already acknowledged message, do nothing
  if (packet.checksum != checksum || packet.acknum < base) {
    // Do nothing
    return;
  }
  int number_of_packets_delivered = packet.acknum - base + 1;
  // increment base if packets are delivered
  if (number_of_packets_delivered > 0) {
    base = packet.acknum + 1;
    // reset base
    if (base == nextseqnum) {
      stoptimer(A);
    } else {
      // restart timer
      stoptimer(A);
      starttimer(A, TIMEOUT);
    }

    // remove messages from buffer
    for (int i = 0; i < number_of_packets_delivered; i++) {
      if (buffered_messages != NULL) {
        buffered_messages = buffered_messages -> next_message;
        continue;
      }
      break;
    }
  }

}

/* called when A's timer goes off */
void A_timerinterrupt() {
  // resent all the messages in the window
  if (buffered_messages != NULL) {
    int seqnum = base;
    struct buffer * temp = buffered_messages;
    while (temp != NULL && seqnum <= base + WINDOWSIZE) {
      struct pkt next_packet;
      next_packet.seqnum = seqnum;
      next_packet.acknum = seqnum;
      memcpy(next_packet.payload, (temp -> message).data, sizeof((temp -> message).data));
      next_packet.checksum = create_checksum(next_packet);
      tolayer3(A, next_packet);
      seqnum++;
      temp = temp -> next_message;
    }
    starttimer(A, TIMEOUT);
  }
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
  buffered_messages = NULL;
  seqnum_A = 0;
  acknum_B = 0;
  TIMEOUT = 50;
  WINDOWSIZE = getwinsize();
  nextseqnum = 0;
  base = 0;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(packet)
struct pkt packet; {

  // Check if corrupted or wrong packet
  int checksum = create_checksum(packet);
  if (packet.checksum != checksum || packet.seqnum != acknum_B) {
    // Resend the last ack
    struct pkt ack;
    ack.seqnum = acknum_B - 1;
    ack.acknum = acknum_B - 1;
    ack.checksum = create_checksum(ack);
    tolayer3(B, ack);
    return;
  }

  // Send data to layer 3
  tolayer5(B, packet.payload);

  // Send ack
  struct pkt ack;
  ack.seqnum = packet.seqnum;
  ack.acknum = packet.seqnum;
  ack.checksum = create_checksum(ack);
  tolayer3(B, ack);
  acknum_B = packet.seqnum + 1;
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init() {}