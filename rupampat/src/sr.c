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
  int seqnum;
  int acked;
  struct buffer *next_message;
} *buffered_messages_A, *buffered_messages_B;

struct timer {
  int seqnum;
  double absolute_interrupt_time;
  struct timer *next_timer;
} * global_logical_timer;

enum host_states {
  waiting_for_acknowledgment,
  waiting_for_packet,
  available
};

enum host_states state_A = available;
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
  for (int i = 0; i<20; i++) {
    checksum += (unsigned char)packet.payload[i];
  }
  return checksum;
}

/* called from layer 5, passed the data to be sent to other side */
void A_output(message)
  struct msg message;
{
  struct buffer *temp = buffered_messages_A;
  int is_first = 1;
  if (temp != NULL) {
    while(temp->next_message != NULL) {
      temp = temp->next_message;
    }
    temp->next_message = malloc(sizeof(struct buffer));
    if (temp->next_message == NULL) {
      // printf("Not enough memory\n");
      exit(0);
    }
    temp->next_message->message = message;
    temp->next_message->next_message = NULL;
    temp->next_message->seqnum = nextseqnum;
    temp->next_message->acked = 0;
    is_first = 0;
  } else {
    buffered_messages_A = malloc(sizeof(struct buffer));
    if (buffered_messages_A == NULL) {
      // printf("Not enough memory\n");
      exit(0);
    }
    buffered_messages_A->message = message;
    buffered_messages_A->next_message = NULL;
    buffered_messages_A->seqnum = nextseqnum;
    buffered_messages_A->acked = 0;
    temp = buffered_messages_A;
  }
  
  if (nextseqnum<base+WINDOWSIZE) {
    struct pkt next_packet;
    next_packet.seqnum = nextseqnum;
    next_packet.acknum = nextseqnum;
    memcpy(next_packet.payload, message.data, sizeof(message.data));
    next_packet.checksum = create_checksum(next_packet);
    if (global_logical_timer == NULL) {
      global_logical_timer = malloc(sizeof(struct timer));
      global_logical_timer->seqnum = nextseqnum;
      global_logical_timer->absolute_interrupt_time = get_sim_time() + TIMEOUT;
      starttimer(A, global_logical_timer->absolute_interrupt_time - get_sim_time());
      // printf("A: Timeout: %f\n", global_logical_timer->absolute_interrupt_time);

    } else {
      struct timer *temp_timer = global_logical_timer;
      while(temp_timer->next_timer != NULL) {
          temp_timer = temp_timer->next_timer;
      }
      temp_timer->next_timer = malloc(sizeof(struct timer));
      temp_timer->next_timer->seqnum = nextseqnum;
      temp_timer->next_timer->absolute_interrupt_time = get_sim_time() + TIMEOUT;
      // printf("A: Timeout: %f\n", global_logical_timer->next_timer->absolute_interrupt_time);
    }
    tolayer3(A, next_packet);
    // printf("A: Sending message \"%s\" with Sequence: %d\n", message.data, nextseqnum);
  }
  nextseqnum++;
}

/* called from layer 3, when a packet arrives for layer 4 */
void A_input(packet)
  struct pkt packet;
{
  // Ack received

  int checksum = create_checksum(packet);
  // printf("A: Receive ack %d %d %d %d %d\n", packet.acknum, base, nextseqnum, packet.checksum, checksum);
  
  // if corrupted do nothing
  if (packet.checksum != checksum || packet.acknum<base) {
    // Do nothing
    return;
  }
  // Acknowledge the packet
  struct buffer *temp = buffered_messages_A;
  while (temp != NULL) {
      if (temp->seqnum == packet.acknum) {
        temp->acked = 1;
        break;
      }
      temp = temp->next_message;
  }

  // from base to latest conseqcutive acknowledgement
  while (buffered_messages_A != NULL && buffered_messages_A->acked == 1) {
    if (buffered_messages_A->seqnum == global_logical_timer->seqnum) {
      global_logical_timer=global_logical_timer->next_timer;
    }
    buffered_messages_A = buffered_messages_A->next_message;
    // printf("Update base");
    base++;
  }
  stoptimer(A);
  if (global_logical_timer !=NULL) {
    starttimer(A, global_logical_timer->absolute_interrupt_time - get_sim_time());
  }

  // struct timer *temp1 = global_logical_timer;
  // while(temp1 != NULL) {
    // printf("TIME %f %d\n",temp1->absolute_interrupt_time, temp1->seqnum);
  //   temp1 = temp1->next_timer;
  // }
}

/* called when A's timer goes off */
void A_timerinterrupt()
{
  // resent only first message from timer
  int current_time = get_sim_time();
  while (global_logical_timer !=NULL && global_logical_timer->absolute_interrupt_time<=get_sim_time()) {
    int seqnum = global_logical_timer->seqnum;
    // printf("A: Interrupt %d\n", seqnum);
    struct buffer *temp_msg = buffered_messages_A;
    while (temp_msg != NULL) {
      if (temp_msg->seqnum==seqnum) {
        break;
      }
      temp_msg = temp_msg->next_message;
    }
    global_logical_timer = global_logical_timer->next_timer;
    if (temp_msg && temp_msg->acked == 0) {
      struct pkt next_packet;
      next_packet.seqnum = seqnum;
      next_packet.acknum = seqnum;
      memcpy(next_packet.payload, (temp_msg->message).data, sizeof((temp_msg->message).data));
      next_packet.checksum = create_checksum(next_packet);
      // printf("A: RESENDING %d\n", next_packet.seqnum);
      tolayer3(A, next_packet);
      if (global_logical_timer != NULL) {
        struct timer *temp = global_logical_timer;
        while(temp->next_timer != NULL) {
          temp = temp->next_timer;
        }
        temp->next_timer = malloc(sizeof(struct timer));
        temp->next_timer->seqnum = seqnum;
        temp->next_timer->absolute_interrupt_time = get_sim_time() + TIMEOUT;
        // printf("TIMEOUTOFFSET %f\n",temp->next_timer->absolute_interrupt_time - get_sim_time());
      } else {
        global_logical_timer = malloc(sizeof(struct timer));
        global_logical_timer->seqnum = seqnum;
        global_logical_timer->absolute_interrupt_time = get_sim_time() + TIMEOUT;
        // printf("TIMEOUTOFFSET %f\n",global_logical_timer->absolute_interrupt_time - get_sim_time());
      }
      starttimer(A, global_logical_timer->absolute_interrupt_time - get_sim_time());
    }
  }



  struct timer *temp = global_logical_timer;
  while(temp != NULL) {
    // printf("TIME %f %d\n",temp->absolute_interrupt_time, temp->seqnum);
    temp = temp->next_timer;
  }
}  

/* the following routine will be called once (only) before any other */
/* entity A routines are called. You can use it to do any initialization */
void A_init()
{
  state_A = available;
  buffered_messages_A = NULL;
  seqnum_A = 0;
  acknum_B = 0;
  TIMEOUT = 100;
  WINDOWSIZE = getwinsize();
  nextseqnum = 0;
  base = 0;
}

/* Note that with simplex transfer from a-to-B, there is no B_output() */

/* called from layer 3, when a packet arrives for layer 4 at B*/
void B_input(packet)
  struct pkt packet;
{
  // printf("B: Receive message %s %d %d\n", packet.payload, packet.seqnum, acknum_B);

  // Check if corrupted or wrong packet
  int checksum = create_checksum(packet);
  if (packet.checksum != checksum) {
    // Do nothing
    // printf("B: Corrupted message; Do nothing");
    return;
  }
  struct pkt ack;
  ack.seqnum = packet.seqnum;
  ack.acknum = packet.seqnum;
  ack.checksum = create_checksum(ack);
  tolayer3(B, ack);
  // printf("B: Successfully send ack %d \n", packet.seqnum);

  
  
  if (packet.seqnum == acknum_B) {
    tolayer5(B, packet.payload);
    acknum_B++;
    while (buffered_messages_B !=NULL && buffered_messages_B->seqnum == acknum_B) {
      tolayer5(B, buffered_messages_B->message.data);
      acknum_B++;
      buffered_messages_B = buffered_messages_B->next_message;
    }
  } else if (packet.seqnum>acknum_B) {
    struct buffer *temp = buffered_messages_B;

    if (temp != NULL) {
      struct buffer *new_msg = malloc(sizeof(struct buffer));
      struct msg m;
      memcpy(m.data, packet.payload, sizeof(m.data));
      new_msg->message = m;
      new_msg->seqnum = packet.seqnum;
      while(temp->next_message != NULL && temp->next_message->seqnum<packet.seqnum) {
        temp = temp->next_message;
      }
      new_msg->next_message = temp->next_message;
      temp->next_message = new_msg;
    } else {
      buffered_messages_B = malloc(sizeof(struct buffer));
      if (buffered_messages_B == NULL) {
        // printf("Not enough memory\n");
        exit(0);
      }
      struct msg m;
      memcpy(m.data, packet.payload, sizeof(m.data));
      buffered_messages_B->message = m;
      buffered_messages_B->next_message = NULL;
      buffered_messages_B->seqnum = packet.seqnum;
    }
  }
  // printf("STORED MESSAGES ON B\n");
  struct buffer *temp = buffered_messages_B;
  while (temp !=NULL) {
    // printf("%s %d\n", temp->message.data, temp->seqnum);
    temp=temp->next_message;
  }
}

/* the following routine will be called once (only) before any other */
/* entity B routines are called. You can use it to do any initialization */
void B_init()
{
  buffered_messages_B = NULL;
}