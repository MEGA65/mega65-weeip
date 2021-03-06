#include <stdio.h>

#include "weeip.h"
#include "eth.h"
#include "arp.h"
#include "dns.h"
#include "dhcp.h"

#include "memory.h"
#include "random.h"

// approx 255ms between DHCP requests
#define DHCP_RETRY_TICKS 255

unsigned char dhcp_configured=0;
unsigned char dhcp_xid[4]={0};

// Share data buffer with DNS client to save space
extern unsigned char dns_query[512];
extern unsigned char dns_buf[1024];
SOCKET *dhcp_socket;

void dhcp_send_query(void);

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
    for(i=0;i<6;i++) if (dhcp_xid[i]!=dns_buf[4+i]) break;
    if (i<4) break;
    // Check that MAC address matches us
    for(i=0;i<6;i++) if (dns_buf[0x1c+i]!=mac_local.b[i]) break;
    if (i<6) break;

    // Check that its a DHCP reply message
    if (dns_buf[0x00]!=0x02) break;
    if (dns_buf[0x01]!=0x01) break;
    if (dns_buf[0x02]!=0x06) break;
    if (dns_buf[0x03]!=0x00) break;
    
    // Ok, its for us. Extract the info we need.
    
    // Set our IP from BOOTP field
    for(i=0;i<4;i++) ip_local.b[i] = dns_buf[0x10+i];
    printf("IP is %d.%d.%d.%d\n",ip_local.b[0],ip_local.b[1],ip_local.b[2],ip_local.b[3]);

    // Only process DHCP fields if magic cookie is set
    if (dns_buf[0xec]!=0x63) break;
    if (dns_buf[0xed]!=0x82) break;
    if (dns_buf[0xee]!=0x53) break;
    if (dns_buf[0xef]!=0x63) break;
    printf("Parsing DHCP fields.\n");

    offset=0xf0;
    while(dns_buf[offset]!=0xff) {
      if (!dns_buf[offset]) offset++;
      else {
	type = dns_buf[offset];
	// Skip field type
	offset++;
	// Parse and skip length marker
	len=dns_buf[offset];
	offset++;
	// offset now points to the data field.
	switch(type) {
	case 0x01:
	  for(i=0;i<4;i++) ip_mask.b[i] = dns_buf[offset+i];	  
	  printf("Netmask is %d.%d.%d.%d\n",ip_mask.b[0],ip_mask.b[1],ip_mask.b[2],ip_mask.b[3]);
	  break;
	case 0x03:
	  for(i=0;i<4;i++) ip_gate.b[i] = dns_buf[offset+i];	  
	  printf("Gateway is %d.%d.%d.%d\n",ip_gate.b[0],ip_gate.b[1],ip_gate.b[2],ip_gate.b[3]);
	  break;
	case 0x06:
	  for(i=0;i<4;i++) ip_dnsserver.b[i] = dns_buf[offset+i];	  
	  printf("DNS is %d.%d.%d.%d\n",ip_dnsserver.b[0],ip_dnsserver.b[1],ip_dnsserver.b[2],ip_dnsserver.b[3]);
	  break;
	default:
	  break;
	}
	// Skip over length and continue
	offset+=len;
      }      
    }

    // XXX We SHOULD send a packet to acknowledge the offer.
    // It works for now without it, because we start responding to ARP requests which
    // sensible DHCP servers will perform to verify occupancy of the IP.
    
    // Mark DHCP configuration complete, and free the socket
    dhcp_configured=1;
    printf("DHCP configuration complete.\n");
    socket_release(dhcp_socket);    
    
    break;
  }

   return 0;
}

byte_t dhcp_autoconfig_retry(byte_t b)
{
  if (!dhcp_configured) {
    
    // This will automatically re-add us to the list
    dhcp_send_query();
    task_add(dhcp_autoconfig_retry, DHCP_RETRY_TICKS, 0);
  }
  return 0;
}

bool_t dhcp_autoconfig(void)
{ 
  if (dhcp_configured) return 1;
  
  dhcp_socket = socket_create(SOCKET_UDP);
  socket_set_callback(dhcp_reply_handler);
  socket_set_rx_buffer(dns_buf,1024);

  dhcp_send_query();

  // Mark ourselves as not yet having configured by DHCP
  dhcp_configured=0;

  // Schedule ourselves to retransmit DHCP query until we are configured
  task_add(dhcp_autoconfig_retry, DHCP_RETRY_TICKS, 0);
  
  
}

void dhcp_send_query(void)
{
  uint16_t dhcp_query_len=0;
  unsigned char field_len;
  unsigned char prefix_position,i;
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

  return ;
}
