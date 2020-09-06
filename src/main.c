
#include <stdio.h>
#include <string.h>

#include "task.h"
#include "weeip.h"
#include "eth.h"
#include "arp.h"

#include "memory.h"
#include "random.h"

void interrupt_handler(void)
{
      tick();
      i_task_cancel(eth_task);
      i_task_add(eth_task, 0, 0);
}

/* No idea what this does. */
byte_t pisca (byte_t p)
{
  // Just adds itself to be run periodically?
   task_add(pisca, 32, !p);
   return 0; // XXX and what should it return?
}

SOCKET *s;
byte_t buf[1024];
byte_t dns_query[512];
uint16_t dns_query_len=0;

/* Function that is used as a call-back on socket events
 * */
byte_t comunica (byte_t p)
{
   socket_select(s);
   switch(p) {
      case WEEIP_EV_CONNECT:
	printf("Saying hello\n");
         buf[0] = 'h';
         buf[1] = 'e';
         buf[2] = 'l';
         buf[3] = 'l';
         buf[4] = 'o';
         socket_send(buf, 5);
         break;
      case WEEIP_EV_DISCONNECT:
         socket_reset();
         socket_listen(55);
         break;
      case WEEIP_EV_DATA:
         buf[0] = 'o';
         buf[1] = 'k';
         socket_send(buf, 2);
         break;
   }

   return 0;
}

byte_t dns_reply_handler (byte_t p)
{
  socket_select(s);
  switch(p) {
  case WEEIP_EV_DATA:
    printf("DNS reply packet received.\n");
    break;
  }
  return 0;
}

void main(void)
{
  IPV4 a;
  EUI48 mac;
  char *hostname="mega65.org";
  unsigned char field_len;
  unsigned char prefix_position,i;
  
  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));
  
   // Set MAC address
   mac_local.b[0] = 0x72;
   mac_local.b[1] = 0xb8;
   mac_local.b[2] = 0x79;
   mac_local.b[3] = 0xb1;
   mac_local.b[4] = 0x36;
   mac_local.b[5] = 0x38;

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

   printf("Calling weeip_init()\n");
   weeip_init();

   // XXX Cause ethernet handler to be added to task list.
   // XXX Should really just get added when we see a packet
   interrupt_handler();   
   
#ifdef TEST_TCP_LISTEN   
   printf("Setting up TCP listen on port 55.\n");
   s = socket_create(SOCKET_TCP);
   socket_set_callback(comunica);
   socket_set_rx_buffer(buf, 1024);
   socket_listen(55);
#endif
#define TEST_DNS_QUERY
#ifdef TEST_DNS_QUERY
   

   s = socket_create(SOCKET_UDP);
   socket_set_callback(dns_reply_handler);
   socket_set_rx_buffer(buf,1024);
   a.b[0]=8;
   a.b[1]=8;
   a.b[2]=8;   
   a.b[3]=8;

   // Before we get any further, send an ARP query for the DNS server
   // (or if it isn't on the same network segment, for our gateway.)
   printf("Performing ARP resolution of DNS server\n");
   arp_query(&a);
   arp_query(&ip_gate);
   // Then wait until we get a reply.
   while((!query_cache(&a,&mac)) &&(!query_cache(&ip_gate,&mac)) ) {
     task_periodic();     
   }   
   printf("Resolved DNS server or gateway to MAC address %02x:%02x:...\n",
	  mac.b[0],mac.b[1]);
   
   socket_select(s);
   socket_connect(&a,53);
   
   // Now form our DNS query
   // 16-bit random request ID:  
   dns_query[0]=random32(256);
   dns_query[1]=random32(256);
   // Request flags: Is request, please recurse etc
   dns_query[2]=0x01;
   dns_query[3]=0x00;
   // QDCOUNT = 1 (one question follows)
   dns_query[4]=0x00;
   dns_query[5]=0x01;
   // ANCOUNT = 0 (no answers follow)
   dns_query[6]=0x00;
   dns_query[7]=0x00;
   // NSCOUNT = 0 (no records follow)
   dns_query[8]=0x00;
   dns_query[9]=0x00;
   // ARCOUNT = 0 (no additional records follow)
   dns_query[10]=0x00;
   dns_query[11]=0x00;
   dns_query_len=12;
   
   // Now convert dotted hostname to DNS field format.
   // This involves changing each . to the length of the following field,
   // adding a $00 to the end, and prefixing the whole thing with the
   // length of the first part.
   // This is most easily done by reserving the prefix byte first, and whenever
   // we hit a . or end of string, updating the previous prefix byte.
   prefix_position=dns_query_len++;   
   lcopy(hostname,&dns_query[dns_query_len],strlen(hostname)+1);
   field_len=0;
   for(i=0;;i++) {
     if (hostname[i]=='.'||hostname[i]==0) {
       dns_query[prefix_position]=field_len;
       prefix_position=dns_query_len+i;
       field_len=0;
     } else field_len++;
     if (hostname[i]==0) break;
   }
   dns_query_len+=strlen(hostname)+1;
   
   socket_send(dns_query,dns_query_len);
#endif
   
   printf("Ready for main loop.\n");
   while(1) {
     // XXX Actually only call it periodically
     task_periodic();
   }
}
