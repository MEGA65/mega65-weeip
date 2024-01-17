#include <stdio.h>
#include <stdint.h>
#ifndef LLVM
#include <stdlib.h>
#endif

#include "weeip.h"
#include "eth.h"
#include "arp.h"
#include "dns.h"
#include "dhcp.h"

#include "mega65/memory.h"
#include "mega65/random.h"

// #define DEBUG_DHCP

// Don't request DHCP retries too quickly
// as the network tick loop may be called
// very fast during configuration
#define DHCP_RETRY_TICKS 255

unsigned char dhcp_configured=0,dhcp_acks=0;
unsigned char dhcp_xid[4]={0};

extern IPV4 ip_broadcast;                  ///< Subnetwork broadcast address
IPV4 ip_dhcpserver;

// Share data buffer with DNS client to save space
// NOTE: dns_query must be at least 512 bytes long
extern unsigned char dns_query[512];
extern unsigned char dns_buf[256];
SOCKET *dhcp_socket;

void dhcp_send_query_or_request(unsigned char requestP);

byte_t dhcp_reply_handler (byte_t p)
{
  unsigned int type,len,offset;
  unsigned int i;

  socket_select(dhcp_socket);
  switch(p) {
  case WEEIP_EV_DATA:
    // First time it will be the offer.
    // And actually, that's all we care about receiving.
    // We MUST however, send the ACCEPT message.
    // Check that XID matches us
    for(i=0;i<6;i++) if (dhcp_xid[i]!=dns_query[4+i]) break;
    if (i<4) break;
    // Check that MAC address matches us
    for(i=0;i<6;i++) if (dns_query[0x1c+i]!=mac_local.b[i]) break;
    if (i<6) break;

    // Check that its a DHCP reply message
    if (dns_query[0x00]!=0x02) break;
    if (dns_query[0x01]!=0x01) break;
    if (dns_query[0x02]!=0x06) break;
    if (dns_query[0x03]!=0x00) break;
    
    // Ok, its for us. Extract the info we need.    

    // Default mask 255.255.255.0
    for(i=0;i<3;i++) ip_mask.b[i] = 0xff; ip_mask.b[3]=0;
    // DNS defaults to the DHCP server, unless specified later in an option
    // (This is required because although we ask for it in our request,
    // Fritz Boxes at least seem to not always provide it, even when asked.)
    for(i=0;i<4;i++) ip_dnsserver.b[i] = dns_query[20+i];
    
    // Set our IP from BOOTP field
    for(i=0;i<4;i++) ip_local.b[i] = dns_query[0x10+i];
#ifdef DEBUG_DHCP
    printf("IP is %d.%d.%d.%d\n",ip_local.b[0],ip_local.b[1],ip_local.b[2],ip_local.b[3]);
#endif
    
    // Only process DHCP fields if magic cookie is set
    if (dns_query[0xec]!=0x63) break;
    if (dns_query[0xed]!=0x82) break;
    if (dns_query[0xee]!=0x53) break;
    if (dns_query[0xef]!=0x63) break;
 
#ifdef DEBUG_DHCP
    //    printf("Parsing DHCP fields $%002x\n",dns_query[0xf2]);
#endif

    if (dns_query[0xf2]==0x02) {
      offset=0xf0;
      while(dns_query[offset]!=0xff&&(offset<512)) {
	if (!dns_query[offset]) offset++;
	else {
	  type = dns_query[offset];
	  // Skip field type
	  offset++;
	  // Parse and skip length marker
	  len=dns_query[offset];
	  offset++;
	  // offset now points to the data field.
	  switch(type) {
	  case 0x01:
	    for(i=0;i<4;i++) ip_mask.b[i] = dns_query[offset+i];	  
#ifdef DEBUG_DHCP
	    printf("Netmask is %d.%d.%d.%d\n",ip_mask.b[0],ip_mask.b[1],ip_mask.b[2],ip_mask.b[3]);
#endif
	    break;
	  case 0x03:
	    for(i=0;i<4;i++) ip_gate.b[i] = dns_query[offset+i];	  
#ifdef DEBUG_DHCP
	    printf("Gateway is %d.%d.%d.%d\n",ip_gate.b[0],ip_gate.b[1],ip_gate.b[2],ip_gate.b[3]);
#endif
	    break;
	  case 0x06:
	    for(i=0;i<4;i++) ip_dnsserver.b[i] = dns_query[offset+i];	  
#ifdef DEBUG_DHCP
	    	    printf("DNS server is %d.%d.%d.%d\n",ip_dnsserver.b[0],ip_dnsserver.b[1],ip_dnsserver.b[2],ip_dnsserver.b[3]);
#endif
	    break;
    case 0x36: // DHCP Server Identifier
      for(i=0;i<4;i++) ip_dhcpserver.b[i] = dns_query[offset+i];
      break;
	  default:
	    break;
	  }
	  // Skip over length and continue
	  offset+=len;
	}      
      }
      
      // Compute broadcast address
      for(i=0;i<4;i++) ip_broadcast.b[i]=(0xff&(0xff^ip_mask.b[i]))|ip_local.b[i];
#ifdef DEBUG_DHCP
      printf("Broadcast is %d.%d.%d.%d\n",ip_broadcast.b[0],ip_broadcast.b[1],ip_broadcast.b[2],ip_broadcast.b[3]);
#endif      
      // XXX We SHOULD send a packet to acknowledge the offer.
      // It works for now without it, because we start responding to ARP requests which
      // sensible DHCP servers will perform to verify occupancy of the IP.
#ifdef DEBUG_DHCP
      //      printf("Sending DHCP ACK\n");
#endif
      dhcp_send_query_or_request(1);
      // Fritz box only sends DHCP ACK, not message type 5, so we just give up
      // after a couple of goes
      dhcp_acks++;
      if (dhcp_acks>2) {
	dhcp_configured=1;
	socket_release(dhcp_socket);
      }

    } else if (dns_query[0xf2]==0x05) {
      // Mark DHCP configuration complete, and free the socket
      dhcp_configured=1;
#ifdef DEBUG_DHCP
      //      printf("DHCP configuration complete.\n");
#endif
      socket_release(dhcp_socket);
    } else {
#ifdef DEBUG_DHCP
      //      printf("Unknown DHCP message\n");
#endif
    }
    
    break;
  }

   return 0;
}

byte_t dhcp_autoconfig_retry(byte_t b)
{
  static char delay = 30;
  if (!dhcp_configured) {
    --delay;
    if (delay == 0) {
      // This will automatically re-add us to the list
      dhcp_send_query_or_request(0);
      delay = 30;
    }
    task_add(dhcp_autoconfig_retry, DHCP_RETRY_TICKS, 0,"dhcprtry");

  }
  return 0;
}

bool_t dhcp_autoconfig(void)
{ 
  if (dhcp_configured) return 1;

  // Initially we have seen zero DHCP acks
  dhcp_acks=0;
  
  dhcp_socket = socket_create(SOCKET_UDP);
  socket_set_callback(dhcp_reply_handler);
  socket_set_rx_buffer((uint32_t)&dns_query[0],sizeof dns_query);

  dhcp_send_query_or_request(0);

  // Mark ourselves as not yet having configured by DHCP
  dhcp_configured=0;

  // Schedule ourselves to retransmit DHCP query until we are configured
  task_add(dhcp_autoconfig_retry, DHCP_RETRY_TICKS, 0,"dhcprtry");
  
  return 0;  
}

void dhcp_send_query_or_request(unsigned char requestP)
{
  uint16_t dhcp_query_len=0;
  unsigned char i;
  IPV4 ip_broadcast;

  socket_select(dhcp_socket);
  for(i=0;i<4;i++) ip_broadcast.b[i]=255;
  socket_connect(&ip_broadcast,67);
  // force local port to 68
  // (reversed byte order for network byte ordering)
  dhcp_socket->port=68<<8;  

  // Now form our DHCP discover message
  // 16-bit random request ID:  
  dns_query[dhcp_query_len++]=0x01; // OP
  dns_query[dhcp_query_len++]=0x01; // HTYPE
  dns_query[dhcp_query_len++]=0x06; // HLEN
  dns_query[dhcp_query_len++]=0x00; // HOPS
  // Transaction ID
  // PGS Generate a single XID we use across multiple requests, to
  // avoid race conditions where a reply might always come just as
  // we have sent of a new request with a different XID.
  // (I have seen this happen quite a number of times).
  if (!dhcp_xid[0]) for(i=0;i<4;i++) dhcp_xid[i]=random32(256);
  for (i=0;i<4;i++) dns_query[dhcp_query_len++]=dhcp_xid[i];  
  dns_query[dhcp_query_len++]=0x00; // SECS since start of request
  dns_query[dhcp_query_len++]=0x00; // SECS since start of request
  dns_query[dhcp_query_len++]=0x00; // FLAGS
  dns_query[dhcp_query_len++]=0x00; // FLAGS
  // Various empty fields  
  for(i=0;i<16;i++) dns_query[dhcp_query_len+i]=0x00;
  if (!requestP) {
  } else {
    // Client IP
    for(i=0;i<4;i++) dns_query[dhcp_query_len+i]=ip_local.b[i];
  }
  dhcp_query_len+=16;
  // our MAC address padded to 192 bytes
  for(i=0;i<6;i++) dns_query[dhcp_query_len++]=mac_local.b[i];
  for(;i<16+192;i++) dns_query[dhcp_query_len++]=0x00;
  // DHCP magic cookie
  dns_query[dhcp_query_len++]=0x63;
  dns_query[dhcp_query_len++]=0x82;
  dns_query[dhcp_query_len++]=0x53;
  dns_query[dhcp_query_len++]=0x63;
  dns_query[dhcp_query_len++]=0x35;
  dns_query[dhcp_query_len++]=0x01;
  if (requestP)
    dns_query[dhcp_query_len++]=0x03;   // DHCP request (i.e., client accepting offer)
  else
    dns_query[dhcp_query_len++]=0x01;   // DHCP discover
  // Pad to word boundary
  dns_query[dhcp_query_len++]=0x00;
  if (requestP) {
    // BOOTP option $32 Confirm IP address
    dns_query[dhcp_query_len++]=0x32;
    dns_query[dhcp_query_len++]=0x04;
    for(i=0;i<4;i++) dns_query[dhcp_query_len++]=ip_local.b[i];
    // BOOTP $36 Confirm DHCP server 
    dns_query[dhcp_query_len++]=0x36;
    dns_query[dhcp_query_len++]=0x04;
    for(i=0;i<4;i++) dns_query[dhcp_query_len++]=ip_dhcpserver.b[i];
  } else {
    // Identify us as 'MEGA65'
    dns_query[dhcp_query_len++]=0x0C;
    dns_query[dhcp_query_len++]=0x06;
    dns_query[dhcp_query_len++]=0x4d;
    dns_query[dhcp_query_len++]=0x45;
    dns_query[dhcp_query_len++]=0x47;
    dns_query[dhcp_query_len++]=0x41;
    dns_query[dhcp_query_len++]=0x36;
    dns_query[dhcp_query_len++]=0x35;
    
    // Request subnetmask, router, domainname, DNS server
    dns_query[dhcp_query_len++]=0x37;
    dns_query[dhcp_query_len++]=0x05;
    dns_query[dhcp_query_len++]=0x01;
    dns_query[dhcp_query_len++]=0x03;
    dns_query[dhcp_query_len++]=0x0f;
    dns_query[dhcp_query_len++]=0x06;
    dns_query[dhcp_query_len++]=0x06;

  }
  // End of request
  dns_query[dhcp_query_len++]=0xff;

  // Pad out to standard length
  while(dhcp_query_len<512)
    dns_query[dhcp_query_len++]=0x00;
  
  socket_send(dns_query,dhcp_query_len);

  return ;
}
