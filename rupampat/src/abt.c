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

/********* STUDENTS WRITE THE NEXT SIX ROUTINES *********/

#define A 0
#define B 1

int create_checksum(struct pkt packet);
void send_next_available_message();

struct buffer {
  struct msg message;
  struct buffer * next_message;
}* buffered_messages;

enum host_states {
  waiting_for_acknowledgment,
  available
};

enum host_states state_A = available;
int seqnum_A;
int acknum_B;
int TIMEOUT;

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

void send_next_available_message() {

  if (buffered_messages != NULL) {
    struct pkt next_packet;
    next_packet.seqnum = seqnum_A;
    next_packet.acknum = seqnum_A;
    memcpy(next_packet.payload, (buffered_messages -> message).data, sizeof((buffered_messages -> message).data));
    next_packet.checksum = create_checksum(next_packet);
    tolayer3(A, next_packet);
    state_A = waiting_for_acknowledgment;
    starttimer(A, TIMEOUT);
  }

}

/* called from layer 5, passed the data to be sent to other side */
// rdt_send(data)
void A_output(message)
struct msg message; {
  struct buffer * temp = buffered_messages;
  if (temp != NULL) {
    while (temp -> next_message != NULL) {
      temp = temp -> next_message;
    }
    temp -> next_message = malloc(sizeof(struct buffer));
    temp -> next_message -> message = message;
    temp -> next_message -> next_message = NULL;
  } else {
    buffered_messages = malloc(sizeof(struct buffer));
    buffered_messages -> message = message;
    buffered_messages -> next_message = NULL;
  }

  if (state_A == waiting_for_acknowledgment) {
    return;
  }
  send_next_available_message();

}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
struct pkt packet; {
  // Ack received
  int checksum = create_checksum(packet);
  if (packet.checksum != checksum || packet.acknum != seqnum_A) {
    // Do nothing
    return;
  }
  seqnum_A = (seqnum_A + 1) % 2;
  stoptimer(A);
  state_A = available;
  // remove message from buffer
  if (buffered_messages != NULL) {
    buffered_messages = buffered_messages -> next_message;
    send_next_available_message();
  }

}

/* called when A's timer goes off */
void A_timerinterrupt() {
  send_next_available_message();
}

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init() {
  state_A = available;
  buffered_messages = NULL;
  seqnum_A = 0;
  acknum_B = 0;
  TIMEOUT = 10;
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
    ack.seqnum = (acknum_B + 1) % 2;
    ack.acknum = (acknum_B + 1) % 2;
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
  // char empty[] = "";
  // memcpy(ack.payload, empty, sizeof(empty));

  ack.checksum = create_checksum(ack);
  tolayer3(B, ack);
  acknum_B = (packet.seqnum + 1) % 2;
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init() {}