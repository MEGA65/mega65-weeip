#include <stdio.h>

#include "weeip.h"
#include "eth.h"
#include "arp.h"
#include "dns.h"
#include "dhcp.h"

#include "memory.h"
#include "random.h"

unsigned char dhcp_configured=0;
unsigned char dhcp_xid[4]={0};

// Share data buffer with DNS client to save space
extern unsigned char dns_query[512];
extern unsigned char dns_buf[1024];
SOCKET *dhcp_socket;

byte_t dhcp_reply_handler (byte_t p)
{
  unsigned int ofs;
  
  socket_select(dhcp_socket);
  switch(p) {
  case WEEIP_EV_DATA:
    // First time it will be the offer.
    // And actually, that's all we care about receiving.
    // We MUST however, send the ACCEPT message.
    break;
  }

  
  // Set our IP
   ip_local.b[0] = 203;
   ip_local.b[1] = 19;
   ip_local.b[2] = 107;
   ip_local.b[3] = 64;

   // Set gateway IP
   ip_gate.b[0] = 203;
   ip_gate.b[1] = 19;
   ip_gate.b[2] = 107;
   ip_gate.b[3] = 1;
      
   // Set Netmask
   ip_mask.b[0] = 255;
   ip_mask.b[1] = 255;
   ip_mask.b[2] = 255;
   ip_mask.b[3] = 0;

   // Set DNS server
   ip_dnsserver.b[0] = 8;
   ip_dnsserver.b[1] = 8;
   ip_dnsserver.b[2] = 8;
   ip_dnsserver.b[3] = 8;

   return 0;
}



bool_t dhcp_autoconfig(void)
{ 
  uint16_t dhcp_query_len=0;
  unsigned char field_len;
  unsigned char prefix_position,i;
  IPV4 ip_broadcast;
  
  dhcp_socket = socket_create(SOCKET_UDP);
  socket_set_callback(dhcp_reply_handler);
  socket_set_rx_buffer(dns_buf,1024);
  
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
  for(i=0;i<4;i++) {
    dhcp_xid[i]=random32(256);
    dns_query[dhcp_query_len++]=dhcp_xid[i];
  }
  dns_query[dhcp_query_len++]=0x00; // SECS since start of request
  dns_query[dhcp_query_len++]=0x00; // SECS since start of request
  dns_query[dhcp_query_len++]=0x00; // FLAGS
  dns_query[dhcp_query_len++]=0x00; // FLAGS
  // Various empty fields
  for(i=0;i<16;i++) dns_query[dhcp_query_len++]=0x00;
  // our MAC address padded to 192 bytes
  for(i=0;i<6;i++) dns_query[dhcp_query_len++]=mac_local.b[i];
  for(;i<16+192;i++) dns_query[dhcp_query_len++]=0x00;
  // DHCP magic cookie
  dns_query[dhcp_query_len++]=0x63;
  dns_query[dhcp_query_len++]=0x82;
  dns_query[dhcp_query_len++]=0x53;
  dns_query[dhcp_query_len++]=0x63;
  // DHCP discover
  dns_query[dhcp_query_len++]=0x35;
  dns_query[dhcp_query_len++]=0x01;
  dns_query[dhcp_query_len++]=0x01;
  // Pad to word boundary
  dns_query[dhcp_query_len++]=0x00;
  // Request subnetmask, router, domainname, DNS server
  dns_query[dhcp_query_len++]=0x37;
  dns_query[dhcp_query_len++]=0x04;
  dns_query[dhcp_query_len++]=0x01;
  dns_query[dhcp_query_len++]=0x03;
  dns_query[dhcp_query_len++]=0x0f;
  dns_query[dhcp_query_len++]=0x06;
  // End of request
  dns_query[dhcp_query_len++]=0xff;

  // Pad out to standard length
  while(dhcp_query_len<512)
    dns_query[dhcp_query_len++]=0x00;
  
  socket_send(dns_query,dhcp_query_len);

  // Mark ourselves as not yet having configured by DHCP
  dhcp_configured=0;

  return 1;
}
