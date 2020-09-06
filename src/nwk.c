/**
 * @file nwk.c
 * @brief Network and Transport layers.
 * @compiler CPIK 0.7.3 / MCC18 3.36
 * @author Bruno Basseto (bruno@wise-ware.org)
 */

#include <stdio.h>

#include "memory.h"

/********************************************************************************
 ********************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 1995-2013 Bruno Basseto (bruno@wise-ware.org).
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ********************************************************************************
 ********************************************************************************/

#include <string.h>
#include "weeip.h"
#include "checksum.h"
#include "eth.h"
#include "arp.h"

#include "memory.h"

/**
 * Message header buffer.
 */
HEADER _header;

static uint16_t data_size;
static _uint32_t seq;
#define TCPH(X) _header.t.tcp.X
#define UDPH(X) _header.t.udp.X
#define IPH(X) _header.ip.X

/**
 * Packet counter.
 */
uint16_t id;

/**
 * Local IP address.
 */
IPV4 ip_local;

/**
 * Default header.
 */
byte_t default_header[] = {
   0x45, 0x08, 0x00, 0x28, 0x00, 0x00, 0x00, 0x00, 0x40, 0x06,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
   0x00, 0x00, 0x50, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00
};

/**
 * Flags received within last packet.
 */
byte_t _flags;

/**
 * Create an IP address from a string like "0.0.0.0"
 * @param str String to convert.
 * @return IP address.
 */
//IPV4
//ip_address(static char rom *str)
//{
//   IPV4 res;
//   char c;
//   register byte_t i;
//
//   res.d = 0;
//   for(i=0; (*str) && (i<4); str++) {
//      c = *str;
//      if(c == '.') {
//         i++;
//         continue;
//      }
//      if(!isdigit(c)) continue;
//      res.b[i] = 10 * res.b[i] + (c - '0');
//   }
//
//   return res;
//}

void dump_bytes(char *msg,uint8_t *d,int count)
{
  printf("%s: ",msg);
  while(count) {
    printf(" %02x",*d);
    d++; count--;
  }
  printf("\n");
}

/**
 * TCP timing control task.
 * Called periodically at a rate defined by TICK_TCP.
 */
byte_t nwk_tick (byte_t sig)
{
   static byte_t t=0;
   
   /*
    * Loop all sockets.
    */
   for_each(_sockets, _sckt) {
      if(_sckt->type != SOCKET_TCP) continue;               // UDP socket or unused.


      if(_sckt->time == 0) continue;                        // does not have timing requirements.

      /*
       * Do socket timing.
       */
      //      _sckt->time--;
      if(_sckt->time == 0) {
         /*
          * Timeout.
          * Check retransmissions.
          */
         if(_sckt->retry) {
            _sckt->retry--;
            _sckt->time = TIMEOUT_TCP;
            switch(_sckt->state) {
               case _SYN_SENT:
               case _ACK_REC:
                  _sckt->toSend = SYN;
                  break;
               case _SYN_REC:
                  _sckt->toSend = SYN | ACK;
                  break;
               case _ACK_WAIT:
                  _sckt->toSend = ACK | PSH;
                  break;
               case _FIN_SENT:
               case _FIN_ACK_REC:
                  _sckt->toSend = ACK;
                  break;
               case _FIN_REC:
                  _sckt->toSend = FIN | ACK;
                  break;
               default:
                  _sckt->time = 0;
                  _sckt->timeout = FALSE;
                  break;
            }
            if(_sckt->toSend) {
               /*
                * Force nwk_upstream() to execute.
                */
               _sckt->timeout = TRUE;
               task_cancel(nwk_upstream);
               task_add(nwk_upstream, 0, 0);
            } 
        } else {
            /*
             * Too much retransmissions.
             * Socket down.
             */
            _sckt->state = _IDLE;
            task_add(_sckt->callback, 0, WEEIP_EV_DISCONNECT);
         }
      }
   } 
   
   /*
    * Reschedule task for periodic execution.
    */
   task_add(nwk_tick, TICK_TCP, 0);
   return 0;
}

/**
 * Network upstream task. Send outgoing network messages.
 */
byte_t nwk_upstream (byte_t sig)
{
   static int i;
   printf("nwk_upstream\n");
   if(!eth_clear_to_send()) {
      /*
       * Ethernet not ready.
       * Delay task execution.
       */
     printf("Waiting for TX clear\n");
      task_add(nwk_upstream, 2, 0);
      return 0;
   }
   
   /*
    * Search for pending messages.
    */
   for_each(_sockets, _sckt) {     
      if(!_sckt->toSend) continue;                          // no message to send for this socket.

      printf("Socket has something to send to $%08lx\n",
	     _sckt->remIP.d);
      /*
       * Pending message found, send it.
       */
      checksum_init();
      // XXX Correct buffer offset processing to handle variable
      // header lengths
      lcopy((uint32_t)default_header,(uint32_t)_header.b,40);

      IPH(id) = HTONS(id);
      id++;

      IPH(source).d = ip_local.d;
      IPH(destination).d = _sckt->remIP.d;
      TCPH(source) = _sckt->port;
      TCPH(destination) = _sckt->remPort;
      
      /*
       * Check payload area in _sckt->tx.
       */
      if(_sckt->toSend & PSH) {
         data_size = _sckt->tx_size;
         ip_checksum((byte_t*)_sckt->tx, data_size);
      } else data_size = 0;
      
      if(_sckt->type == SOCKET_TCP) {
         /*
          * TCP message header.
          */
         IPH(length) = HTONS((40 + data_size));
         TCPH(flags) = _sckt->toSend;

         /*
          * Check sequence numbers.
          */
         seq.d = _sckt->seq.d;
         if(_sckt->timeout) {
            /*
             * Retransmission.
             * Use old sequence number.
             */
            if(data_size) seq.d -= data_size;
            if(_sckt->toSend & (SYN | FIN)) seq.d--;
         }

         TCPH(n_seq).b[0] = seq.b[3];
         TCPH(n_seq).b[1] = seq.b[2];
         TCPH(n_seq).b[2] = seq.b[1];
         TCPH(n_seq).b[3] = seq.b[0];
         TCPH(n_ack).b[0] = _sckt->remSeq.b[3];
         TCPH(n_ack).b[1] = _sckt->remSeq.b[2];
         TCPH(n_ack).b[2] = _sckt->remSeq.b[1];
         TCPH(n_ack).b[3] = _sckt->remSeq.b[0];

         if(!_sckt->timeout) {
            /*
             * Update sequence number data.
             */
            if(data_size) seq.d += data_size;
            if(_sckt->toSend & (SYN | FIN)) seq.d++;
            _sckt->seq.d = seq.d;
         }

         /*
          * Update TCP checksum information.
          */
         TCPH(checksum) = 0;
         ip_checksum(&_header.b[12], 8 + sizeof(TCP_HDR));
         add_checksum(IP_PROTO_TCP);
         add_checksum(data_size + sizeof(TCP_HDR));
         TCPH(checksum) = checksum_result();
      } else {
         /*
          * UDP message header.
          */
         IPH(protocol) = IP_PROTO_UDP;
         IPH(length) = HTONS((28 + data_size));
         UDPH(length) = HTONS((8 + data_size));

         /*
          * Update UDP checksum information.
          */
         UDPH(checksum) = 0;
         ip_checksum(&_header.b[12], 8 + sizeof(UDP_HDR));
         add_checksum(IP_PROTO_UDP);
         add_checksum(data_size + sizeof(UDP_HDR));
         UDPH(checksum) = checksum_result();

         /*
          * Tell UDP that data was sent (no acknowledge).
          */
         task_add(_sckt->callback, 0, WEEIP_EV_DATA_SENT);
      }
      
      /*
       * Update IP checksum information.
       */
      checksum_init();
      ip_checksum((byte_t*)&_header, 20);
      IPH(checksum) = checksum_result();
      
      /*
       * Send IP packet.
       */
      printf("about to call eth_ip_send()\n");
      if(eth_ip_send()) {
	//	printf("eth_ip_send() success\n");
         if(data_size) eth_write((byte_t*)_sckt->tx, data_size);
         eth_packet_send();	 
      }
      _sckt->toSend = 0;
      _sckt->timeout = FALSE;
      _sckt->time = TIMEOUT_TCP;

      /*
       * Reschedule 50ms later for eventual further processing.
       */
      task_add(nwk_upstream, 5, 0);
   }
   
   /*
    * Job done, finish.
    */
   return 0;
}

/**
 * Network downstream processing.
 * Parse incoming network messages.
 */
void nwk_downstream(void)
{
   WEEIP_EVENT ev;

   ev = WEEIP_EV_NONE;

   /*
    * Packet size.
    */
   if(IPH(ver_length) != 0x45) goto drop;
   data_size = IPH(length);
   data_size = NTOHS(data_size);

   /*
    * Checksum.
    */
   checksum_init();
   ip_checksum((byte_t*)&_header, 20);
   if(chks.u != 0xffff) goto drop;

   /*
    * Destination address.
    */
   if(IPH(destination).d != 0xffffffffL)                       // broadcast.
      if(IPH(destination).d != ip_local.d)                     // unicast.
         goto drop;                                            // not for us.

   if (0) printf("IP is for us. Source is %08lx\n",IPH(source).d);
   
   /*
    * Search for a waiting socket.
    */
   for_each(_sockets, _sckt) {
      if(_sckt->type == SOCKET_FREE) continue;                 // unused socket.
      if(_sckt->port != TCPH(destination)) continue;           // another port.
      if(_sckt->type == SOCKET_UDP) {                          // another protocol.
         if(IPH(protocol) != IP_PROTO_UDP) continue;
      } else {
	if(IPH(protocol) != IP_PROTO_TCP) continue;
      }
      if(_sckt->listening) goto found;                         // waiting for a connection.
      if(_sckt->remIP.d != IPH(source).d) continue;            // another source.
      if(_sckt->remPort != TCPH(source)) continue;             // another port.
      printf("Socket matches.\n");
      goto found;                                              // found!
   }

   goto drop;                                                  // no socket for the message.

found:
   /*
    * Update socket data.
    */
   printf("found socket: source.d=$%08lx\n",IPH(source).d);
   _sckt->remIP.d = IPH(source).d;
   _sckt->remPort = TCPH(source);
   _sckt->listening = FALSE;

   if(IPH(protocol) == IP_PROTO_TCP) goto parse_tcp;

   /*
    * UDP message.
    * Copy data into user socket buffer.
    * Add task for processing.
    */
   data_size -= 28;
   if(_sckt->rx) {
      if(data_size > _sckt->rx_size) data_size = _sckt->rx_size;
      lcopy(ETH_RX_BUFFER+2+14+sizeof(IP_HDR)+28,(uint32_t)_sckt->rx, data_size);
      _sckt->rx_data = data_size;
   }
   
   ev = WEEIP_EV_DATA;
   goto done;

parse_tcp:

   printf("parse_tcp\n");
   
   /*
    * TCP message.
    * Check flags.
    */
   _flags = 0;
   data_size -= 40;
   if(TCPH(flags) & RST) {
      /*
       * RST flag received. Force disconnection.
       */
      _sckt->state = _IDLE;
      ev = WEEIP_EV_DISCONNECT;
      goto done;
   }
   
   if(TCPH(flags) & ACK) {
      /*
       * Test acked sequence number.
       */
      if((_sckt->seq.b[0] != TCPH(n_ack).b[3])
          || (_sckt->seq.b[1] != TCPH(n_ack).b[2])
          || (_sckt->seq.b[2] != TCPH(n_ack).b[1])
          || (_sckt->seq.b[3] != TCPH(n_ack).b[0])) {
           /*
            * Out of order, drop it.
            */
           goto drop;
      }
      _flags |= ACK;
   }

   if(TCPH(flags) & SYN) {
      /*
       * Restart of remote sequence number (connection?).
       */
      _sckt->remSeq.b[0] = TCPH(n_seq).b[3];
      _sckt->remSeq.b[1] = TCPH(n_seq).b[2];
      _sckt->remSeq.b[2] = TCPH(n_seq).b[1];
      _sckt->remSeq.b[3] = TCPH(n_seq).b[0];
      _sckt->remSeq.d++;
      _flags |= SYN;
   } else {
      /*
       * Test remote sequence number.
       */
      if((TCPH(n_seq.b[0]) != _sckt->remSeq.b[3])
         || (TCPH(n_seq.b[1]) != _sckt->remSeq.b[2])
         || (TCPH(n_seq.b[2]) != _sckt->remSeq.b[1])
         || (TCPH(n_seq.b[3]) != _sckt->remSeq.b[0])) {
         if(data_size) {
            /*
             * Out of order, send our number.
             */
            _sckt->toSend = ACK;
            task_cancel(nwk_upstream);
            task_add(nwk_upstream, 0, 0);
         }
         goto drop;
      }
      
      /*
       * Update stream sequence number.
       */
      _sckt->remSeq.d += data_size;
   }
   
   if(TCPH(flags) & FIN) {
      _sckt->remSeq.d++;
      _flags |= FIN;
   }

   /*
    * TCP state machine implementation.
    */
   printf("TCP state machine. state=%d\n",_sckt->state);
   switch(_sckt->state) {
      case _LISTEN:
         if(_flags & SYN) {
            /*
             * Start incoming connection procedure.
             */
            _sckt->state = _SYN_REC;
            _sckt->toSend = SYN | ACK;
         }
         break;
         
      case _SYN_SENT:
         if(_flags & (ACK | SYN)) {
            /*
             * Connection established.
             */
            _sckt->state = _CONNECT;
            _sckt->toSend = ACK;
            ev = WEEIP_EV_CONNECT;
            break;
         }

         if(_flags & SYN) {
            _sckt->state = _SYN_REC;
            _sckt->toSend = SYN | ACK;
         }         
         break;
      
      case _SYN_REC:
         if(_flags & ACK) {
            /*
             * Connection established.
             */
            _sckt->state = _CONNECT;
            ev = WEEIP_EV_CONNECT;
         }
         break;

      case _CONNECT:
      case _ACK_WAIT:
         if(_flags & FIN) {
            /*
             * Start remote disconnection procedure.
             */
            _sckt->state = _FIN_REC;
            _sckt->toSend = ACK | FIN;
            ev = WEEIP_EV_DISCONNECT;           // TESTE
            break;
         }

         if((_flags & ACK) && (_sckt->state == _ACK_WAIT)) {
            /*
             * The peer acknowledged the previously sent data.
             */
            _sckt->state = _CONNECT;
            task_add(_sckt->callback, 0, WEEIP_EV_DATA_SENT);
         }         

         if(data_size) {
            /*
             * Data received.
             * Copy data into user socket buffer.
             */
            if(_sckt->rx) {
               if(data_size > _sckt->rx_size) 
                  data_size = _sckt->rx_size;
	       // XXX Correct buffer offset processing to handle variable
	       // header lengths
               lcopy(ETH_RX_BUFFER+68,(uint32_t)_sckt->rx, data_size);
               _sckt->rx_data = data_size;
            }
            _sckt->toSend = ACK;            
            ev = WEEIP_EV_DATA;
         }
         break;
         
      case _FIN_SENT:
         if(_flags & (FIN | ACK)) {
            /*
             * Disconnection done.
             */
            _sckt->state = _IDLE;
            _sckt->toSend = ACK;               
            ev = WEEIP_EV_DISCONNECT;
            break;
         }

         if(_flags & FIN) {
            _sckt->state = _FIN_REC;
            _sckt->toSend = ACK;
            break;
         }

         if(_flags & ACK) {
            _sckt->state = _FIN_ACK_REC;
         }
         break;

      case _FIN_REC:
         if(_flags & ACK) {
            /*
             * Disconnection done.
             */
            _sckt->state = _IDLE;
            ev = WEEIP_EV_DISCONNECT;
         }
         break;

      case _FIN_ACK_REC:
         if(_flags & FIN) {
            /*
             * Disconnection done.
             */
            _sckt->state = _IDLE;
            _sckt->toSend = ACK;
            ev = WEEIP_EV_DISCONNECT;
         }         
         break;
         
      default:
         break;
   }

done:
   /*
    * Verify if there are messages to send.
    * Add nwk_upstream() to send messages.
    */
   if(_sckt->toSend) {
      _sckt->retry = RETRIES_TCP;
      task_cancel(nwk_upstream);
      task_add(nwk_upstream, 0, 0);
   }

   /*
    * Verify event processing.
    * Add socket management task.
    */
   if(ev != WEEIP_EV_NONE)
      task_add(_sckt->callback, 0, ev);

drop:
   return;
}
