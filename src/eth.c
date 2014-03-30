/**
 * @file eth.c
 * @brief Ethernet management for the PIC18F67J60 chip.
 * @compiler CPIK 0.7.3 / MCC18 3.36
 * @author Bruno Basseto (bruno@wise-ware.org)
 */

#include <string.h>
#if defined(__CPIK__)
#include <device/p18F97J60.h>
#include <interrupt.h>
#else
#include <p18F97J60.h>
#endif
//#include <delays.h>
#include "task.h"
#include "weeip.h"
#include "arp.h"

#define _PROMISCUOUS

IPV4 ip_mask;                       ///< Subnetwork address mask.
IPV4 ip_gate;                       ///< IP Gateway address.

/*
 * Ethernet controller memory map.
 */
#define TXBEGIN       0x0000       ///< Transmission buffer start (must be even).
#define RXBEGIN       0x05f8       ///< Reception buffer start (must be even).
#define RXEND         0x1fff       ///< Reception buffer end.

/*
 * Physical registers addresses.
 */
#define PHCON1         0x00
#define PHCON2         0x10
#define PHLCON         0x14

/**
 * Ethernet controller header.
 */
typedef struct {
   uint16_t next;                ///< Pointer to the next packet in memory (little-endian).
   uint16_t size;                ///< Packet size (little endian).
   byte_t status[2];             ///< Packet status.
} CTRL_HEADER;
CTRL_HEADER ctrl_header;

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

static uint16_t next;            // Pointer to the next packet.
static uint16_t eth_size;        // Packet size.

#define write_reg(X,Y) X=Y; nop()
#define eth(X) EDATA=X; nop()

/**
 * Change PHY register.
 * @param addr PHY register address.
 * @param val PHY register new value.
 */
void 
phy_write
   (byte_t addr,
   uint16_t val)
{
   write_reg(MIREGADR, addr);
   write_reg(MIWRL, LOW(val));
   write_reg(MIWRH, HIGH(val));
   while(MISTATbits.BUSY);
}   

/**
 * Tranfer data from ETH memory to RAM memory.
 * @param dest Destination address.
 * @param size Number of bytes to transfer.
 */
void 
eth_read
   (buffer_t dest,
   uint16_t size)
{
   while(size) {
      *dest = EDATA;
      dest++;
      size--;
   }
}

/**
 * Transfer data from RAM memory to ETH memory.
 * @param src Source address.
 * @param size Number of bytes to transfer.
 */
void
eth_write
   (buffer_t src,
   uint16_t size)
{
   while(size) {
      EDATA = *src;
      src++;
      size--;
   }
}

/**
 * Fill ETH memory with a value.
 * @param v Value to write.
 * @param size Number of bytes to fill.
 */
void 
eth_set
   (byte_t v,
   uint16_t size)
{
   while(size) {
      EDATA = v;
      size--;
   }
}

/**
 * Check if the transceiver is ready to transmit a packet.
 * @return TRUE if a packet can be sent.
 */
bool_t
eth_clear_to_send()
{
   if(ECON1bits.TXRTS) return FALSE;               // transmission in progress...
   return TRUE;
}

/**
 * Command the ethernet controller to drop the current package.
 * Select next one, if existing.
 */
void 
eth_drop()
{
   uint16_t p;

   /*
    * Point to next packet.
    */
   next = ctrl_header.next;
   
   /*
    * Free ETH memory.
    */
   if(next == RXBEGIN) p = RXEND;
   else p = next - 1;
   write_reg(ERXRDPTL, LOW(p));
   write_reg(ERXRDPTH, HIGH(p));

   /*
    * Decrement packet counter.
    * Eventually clears the interrupt flag.
    */
   ECON2bits.PKTDEC = 1;
}

/**
 * Ethernet control task.
 * Shall be called when a packet arrives.
 */
byte_t 
eth_task
   (byte_t p)
{
   EIRbits.RXERIF = 0;
   EIRbits.TXERIF = 0;

   /*
    * Check if there are incoming packets.
    */
   if(EPKTCNT == 0) {
      PIE2bits.ETHIE = 1;
      task_add(eth_task, 10, 0);
      return 0;
   }

   /*
    * A packet is available.
    * Read its control header.
    */
   write_reg(ERDPTL, LOW(next));
   write_reg(ERDPTH, HIGH(next));
   eth_read((byte_t*)&ctrl_header, sizeof(ctrl_header));
         
   /*
    * Read packet ethernet header.
    */
   eth_read((byte_t*)&eth_header, sizeof(eth_header));
         
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
      eth_read((byte_t*)&_header, sizeof(ARP_HDR));
      arp_mens();   
      goto drop;
   }
      
   if(eth_header.type == 0x0008) {            // big-endian for 0x0800
      /*
       * IP packet.
       * Verify transport protocol to load header.
       */
      update_cache(&_header.ip.source, &eth_header.source);
      eth_read((byte_t*)&_header, sizeof(IP_HDR));
      switch(_header.ip.protocol) {
         case IP_PROTO_UDP:
            eth_read((byte_t*)&_header.t.udp, sizeof(UDP_HDR));
            break;
         case IP_PROTO_TCP:
            eth_read((byte_t*)&_header.t.tcp, sizeof(TCP_HDR));
            break;
         default:
            goto drop;
      }
      nwk_downstream();
   }

drop:
   eth_drop();
   task_add(eth_task, 0, 0);                    // try again to check more packets.
   return 0;
}

#define IPH(X) _header.ip.X

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

   if(ECON1bits.TXRTS) return FALSE;               // another transmission in progress, fail.
   
   /*
    * Check destination IP.
    */
   ip.d = IPH(destination).d;
   if(ip.d != 0xffffffff) {                        // is it broadcast?
      if(ip_mask.d & (ip.d ^ ip_local.d))          // same network?
         ip.d = ip_gate.d;                         // send to gateway for reaching other networks.
   }

   if(!query_cache(&ip, &mac)) {                   // find MAC
      arp_query(&ip);                              // yet unknown IP, query MAC and fail.
      return FALSE;
   }

   /*
    * Setup ethernet controller TX buffers.
    */
   write_reg(ETXSTL, LOW(TXBEGIN));
   write_reg(ETXSTH, HIGH(TXBEGIN));
   write_reg(EWRPTL, LOW(TXBEGIN));
   write_reg(EWRPTH, HIGH(TXBEGIN));

   /*
    * Send ethernet header.
    */
   eth(0x00);                                      // control byte
   eth_write((byte_t*)&mac, 6);
   eth_write((byte_t*)&mac_local, 6);
   eth(0x08);                                      // type = IP (0x0800)
   eth(0x00);

   /*
    * Send protocol header.
    */
   if(IPH(protocol) == IP_PROTO_UDP) eth_size = 28;    // header size
   else eth_size = 40;
   eth_write((byte_t*)&_header, eth_size);
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
   if(ECON1bits.TXRTS) return;                     // another transmission in progress.
   
   /*
    * Setup ethernet controller TX buffers.
    */
   write_reg(ETXSTL, LOW(TXBEGIN));
   write_reg(ETXSTH, HIGH(TXBEGIN));
   write_reg(EWRPTL, LOW(TXBEGIN));
   write_reg(EWRPTH, HIGH(TXBEGIN));

   /*
    * Send ethernet header.
    */
   eth(0x00);                                      // control byte
   eth_write((byte_t*)mac, 6);
   eth_write((byte_t*)&mac_local, 6);
   eth(0x08);                                      // type = ARP (0x0806)
   eth(0x06);

   /*
    * Send protocol header.
    */
   eth_write((byte_t*)&_header, sizeof(ARP_HDR));
   
   /*
    * Start transmission.
    */
   eth_size = sizeof(ARP_HDR);
   eth_size = TXBEGIN + eth_size + 15 - 1;
   write_reg(ETXNDL, LOW(eth_size));
   write_reg(ETXNDH, HIGH(eth_size));
   EIRbits.TXIF_EIR = 0;
   ECON1bits.TXRTS = 1;
}

/**
 * Finish transfering an IP packet to the ethernet controller and start transmission.
 * @param size Payload size (without headers).
 */
void
eth_packet_send
   (uint16_t size)
{
   eth_size = TXBEGIN + eth_size + size + 15 - 1;
   write_reg(ETXNDL, LOW(eth_size));
   write_reg(ETXNDH, HIGH(eth_size));
   EIRbits.TXIF_EIR = 0;
   ECON1bits.TXRTS = 1;
}

/**
 * Ethernet controller initialization and configuration.
 */
void
eth_init()
{
   uint16_t t;

   ECON1 = 0;
   ECON2 = 0;
   next = RXBEGIN;
   ECON2bits.ETHEN = 1;                  // enable ethernet controller.

   /*
    * Wait for clock stabilization.
    */
   while(!ESTATbits.PHYRDY);
   for(t=0; t<50000UL; t++);
//   Delay1KTCYx(10);                        // delay 1ms

   /*
    * Setup memory map.
    */
   ECON2bits.AUTOINC = 1;
   write_reg(ERXSTL, LOW(RXBEGIN));
   write_reg(ERXSTH, HIGH(RXBEGIN));
   write_reg(ERXRDPTL, LOW(RXEND));
   write_reg(ERXRDPTH, HIGH(RXEND));
   write_reg(ERXNDL, LOW(RXEND));
   write_reg(ERXNDH, HIGH(RXEND));
   write_reg(ETXSTL, LOW(TXBEGIN));
   write_reg(ETXSTH, HIGH(TXBEGIN));
   
   /*
    * Startup MAC layer.
    */
   MACON1bits.MARXEN = 1; nop();
   MACON3 = 0b00110000; nop();
   MACON4bits.DEFER = 1; nop();
   write_reg(MAMXFLL, LOW(1518));
   write_reg(MAMXFLH, HIGH(1518));
   write_reg(MABBIPG, 0x12);
   write_reg(MAIPGL, 0x12);
   write_reg(MAIPGH, 0x0c);

   /*
    * Setup frame reception filter.
    */
#if defined(_PROMISCUOUS)
   write_reg(ERXFCON, 0b00100000);
#else
   write_reg(ERXFCON, 0b10100001);
#endif

   /*
    * Configure MAC address.
    */
   write_reg(MAADR1, mac_local.b[0]);
   write_reg(MAADR2, mac_local.b[1]);
   write_reg(MAADR3, mac_local.b[2]);
   write_reg(MAADR4, mac_local.b[3]);
   write_reg(MAADR5, mac_local.b[4]);
   write_reg(MAADR6, mac_local.b[5]);

   /*
    * Startup PHY layer.
    */
   phy_write(PHCON1, 0b0000000000000000);
   phy_write(PHCON2, 0b0000000100010000);

   /*
    * Enable packet reception.
    */
   ECON1bits.RXEN = 1;
   
   /*
    * Setup interrupt services.
    */
   EIR = 0;
   EIE = 0b01000000;
   PIR2bits.ETHIF = 0;
   PIE2bits.ETHIE = 1;
}

/**
 * Disable ethernet controller.
 */
void eth_disable()
{
   /*
    * Wait for any pending activity.
    */
   disable();
   while(ECON1bits.TXRTS);
   while(ESTATbits.RXBUSY);
   
   /*
    * Disable packet reception.
    * Disable interrupts.
    */
   ECON1bits.RXEN = 0;
   EIR = 0;
   EIE = 0;
   PIR2bits.ETHIF = 0;
   PIE2bits.ETHIE = 0;
   enable();
}

/**
 * Enable ethernet controller.
 */
void eth_enable()
{
   ECON1bits.RXEN = 1;
   EIR = 0;
   EIE = 0b01000000;
   PIR2bits.ETHIF = 0;
   PIE2bits.ETHIE = 1;
}
