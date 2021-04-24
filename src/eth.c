/**
 * @file eth.c
 * @brief Ethernet management for the PIC18F67J60 chip.
 * @compiler CPIK 0.7.3 / MCC18 3.36
 * @author Bruno Basseto (bruno@wise-ware.org)
 */

#include <stdint.h>
#include <stdio.h>

#include <string.h>
#include "task.h"
#include "weeip.h"
#include "arp.h"
#include "eth.h"

#include "memory.h"
#include "hal.h"

#define _PROMISCUOUS

static uint16_t eth_size;        // Packet size.
uint16_t eth_tx_len=0;           // Bytes written to TX buffer


IPV4 ip_mask;                       ///< Subnetwork address mask.
IPV4 ip_gate;                       ///< IP Gateway address.
IPV4 ip_dnsserver;                  ///< DNS Server IP

/**
 * Ethernet frame header.
 */
typedef struct {
   EUI48 destination;            ///< Packet Destination address.
   EUI48 source;                 ///< Packet Source address.
   uint16_t type;                ///< Packet Type or Size (big-endian)
} ETH_HEADER;

/**
 * Ethernet frame header buffer.
 */
ETH_HEADER eth_header;

/**
 * Local MAC address.
 */
EUI48 mac_local;

#define MTU 2048
unsigned char tx_frame_buf[MTU];

void eth(uint8_t b)
{
  if (eth_tx_len<MTU) tx_frame_buf[eth_tx_len]=b;
  eth_tx_len++;
}

/*
 * Check if the transceiver is ready to transmit a packet.
 * @return TRUE if a packet can be sent.
 */
bool_t
eth_clear_to_send()
{
  if(PEEK(0xD6E0)&0x80) {
    return TRUE;
  }
  return FALSE;
}

/**
 * Command the ethernet controller to discard the current frame in the
 * RX buffer.
 * Select next one, if existing.
 */
void 
eth_drop()
{
  // Do nothing, as we pop the ethernet buffer off when asking for a frame in
  // eth_task().
}

/**
 * Ethernet control task.
 * Shall be called when a packet arrives.
 */
uint8_t eth_task (uint8_t p)
{
  /*
   * Check if there are incoming packets.
   * If not, then check in a while.
   */  

  if(!(PEEK(0xD6E1)&0x20)) {
    task_add(eth_task, 10, 0);
    return 0;
  }

  // Get next received packet
  // Just $01 and $03 should be enough, but then packets may be received in triplicate
  // based on testing in wirekrill. But clearing bit 1 again solves this problem.
  POKE(0xD6E1,0x01); POKE(0xD6E1,0x03); POKE(0xD6E1,0x01);
  
  /*
   * A packet is available.
   */
  // +2 to skip length and flags field
  lcopy(ETH_RX_BUFFER+2L,(uint32_t)&eth_header, sizeof(eth_header));
  
  /*
   * Check destination address.
   */

  if((eth_header.destination.b[0] &
      eth_header.destination.b[1] &
      eth_header.destination.b[2] &
      eth_header.destination.b[3] &
      eth_header.destination.b[4] &
      eth_header.destination.b[5]) != 0xff) {
    /*
     * Not broadcast, check if it matches the local address.
     */
    if(memcmp(&eth_header.destination, &mac_local, sizeof(EUI48)))
      goto drop;
  }
  
  /*
   * Address match, check protocol.
   * Read protocol header.
   */
  if(eth_header.type == 0x0608) {            // big-endian for 0x0806
    /*
     * ARP packet.
     */
    lcopy(ETH_RX_BUFFER+2+14,(uint32_t)&_header, sizeof(ARP_HDR));
    arp_mens();   
    goto drop;
  }
  else if(eth_header.type == 0x0008) {            // big-endian for 0x0800
    /*
     * IP packet.
     * Verify transport protocol to load header.
     */
    lcopy(ETH_RX_BUFFER+2+14,(uint32_t)&_header, sizeof(IP_HDR));
    update_cache(&_header.ip.source, &eth_header.source);
    switch(_header.ip.protocol) {
    case IP_PROTO_UDP:
      lcopy(ETH_RX_BUFFER+2+14+sizeof(IP_HDR),(uint32_t)&_header.t.udp, sizeof(UDP_HDR));
      break;
    case IP_PROTO_TCP:
      lcopy(ETH_RX_BUFFER+2+14+sizeof(IP_HDR),(uint32_t)&_header.t.tcp, sizeof(TCP_HDR));
      break;
    case IP_PROTO_ICMP:
      lcopy(ETH_RX_BUFFER+2+14+sizeof(IP_HDR),(uint32_t)&_header.t.icmp, sizeof(ICMP_HDR));
      break;
    default:
      goto drop;
    }

    nwk_downstream();
  }
  else {
    //    printf("Unknown ether type $%04x\n",eth_header.type);
  }
  
 drop:
  eth_drop();
  // We processed a packet, so schedule ourselves immediately, in case there
  // are more packets coming.
  task_add(eth_task, 0, 0);                    // try again to check more packets.
  return 0;
}

#define IPH(X) _header.ip.X

void eth_write(uint8_t *buf,uint16_t len)
{
  if (len+eth_tx_len>=MTU) return;
  lcopy((uint32_t)buf,&tx_frame_buf[eth_tx_len],len);
  eth_tx_len+=len;
}

void dump_bytes(char *msg,uint8_t *d,int count);

/**
 * Finish transfering an IP packet to the ethernet controller and start transmission.
 */
void eth_packet_send(void)
{

  // Set packet length
  mega65_io_enable();
  POKE(0xD6E2,eth_tx_len&0xff);
  POKE(0xD6E3,eth_tx_len>>8);

  // Copy our working frame buffer to 
  lcopy((unsigned long)tx_frame_buf,ETH_TX_BUFFER,eth_tx_len);

#if 0
  printf("ETH TX: %x:%x:%x:%x:%x:%x\n",
	 tx_frame_buf[0],tx_frame_buf[1],tx_frame_buf[2],tx_frame_buf[3],tx_frame_buf[4],tx_frame_buf[5]
	 );
#endif
  
  // Make sure ethernet is not under reset
  POKE(0xD6E0,0x03);
  
  // Send packet
  POKE(0xD6E4,0x01); // TX now
}


/**
 * Start transfering an IP packet.
 * Find MAC address and send headers to the ethernet controller.
 * @return TRUE if succeeded.
 */
bool_t 
eth_ip_send()
{
   static IPV4 ip;
   static EUI48 mac;

   if(!eth_clear_to_send()) {
     return FALSE;               // another transmission in progress, fail.
   }

   /*
    * Check destination IP.
    */
   ip.d = IPH(destination).d;
   if(ip.d != 0xffffffff) {                        // is it broadcast?
      if(ip_mask.d & (ip.d ^ ip_local.d))          // same network?
	{
	  ip.d = ip_gate.d;                         // send to gateway for reaching other networks.
	}
   }

   if(!query_cache(&ip, &mac)) {                   // find MAC
      arp_query(&ip);                              // yet unknown IP, query MAC and fail.
      return FALSE;
   }

   /*
    * Send ethernet header.
    */
   eth_tx_len=0;

   eth_write((uint8_t*)&mac, 6);
   eth_write((uint8_t*)&mac_local, 6);
   eth(0x08);                                      // type = IP (0x0800)
   eth(0x00);

   /*
    * Send protocol header.
    */
   if(IPH(protocol) == IP_PROTO_UDP) eth_size = 28;    // header size
   else eth_size = 40;
   eth_write((uint8_t*)&_header, eth_size);
   
   //   printf("eth_ip_send success.\n");
   return TRUE;
}

/**
 * Send an ARP packet.
 * @param mac Destination MAC address.
 */
void 
eth_arp_send
   (EUI48 *mac)
{
  if(!(PEEK(0xD6E0)&0x80)) return;                     // another transmission in progress.
   
   eth_tx_len=0;

   eth_write((uint8_t*)mac, 6);
   eth_write((uint8_t*)&mac_local, 6);
   eth(0x08);                                      // type = ARP (0x0806)
   eth(0x06);

   /*
    * Send protocol header.
    */
   eth_write((uint8_t*)&_header, sizeof(ARP_HDR));
   
   /*
    * Start transmission.
    */
   eth_packet_send();
}

/**
 * Ethernet controller initialization and configuration.
 */
void
eth_init()
{
   uint16_t t;

   eth_drop();

   /*
    * Setup frame reception filter.
    */
#if defined(_PROMISCUOUS)
   POKE(0xD6E5,PEEK(0xD6E5)&0xFE);
#else
   POKE(0xD6E5,PEEK(0xD6E5)|0x01);
#endif

   // Set ETH TX Phase to 1
   POKE(0xD6E5,(PEEK(0xD6E5)&0xf3)|(1<<2));   
   
   /*
    * Configure MAC address.
    */
   POKE(0xD6E9,mac_local.b[0]);
   POKE(0xD6EA,mac_local.b[1]);
   POKE(0xD6EB,mac_local.b[2]);
   POKE(0xD6EC,mac_local.b[3]);
   POKE(0xD6ED,mac_local.b[4]);
   POKE(0xD6EE,mac_local.b[5]);

   // Reset, then release from reset and reset TX FSM
   POKE(0xd6e0,0);
   POKE(0xd6e0,3);
   POKE(0xd6e1,3);
   POKE(0xd6e1,0);
   
   // XXX Enable ethernet IRQs?
}

/**
 * Disable ethernet controller.
 */
void eth_disable()
{
   /*
    * Wait for any pending activity.
    */
   // XXX Disable ethernet IRQs?
}

