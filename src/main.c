
#include <stdio.h>
#include <string.h>

#include "task.h"
#include "weeip.h"
#include "eth.h"
#include "arp.h"
#include "dns.h"
#include "dhcp.h"

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

void dump_bytes(char *msg,uint8_t *d,int count);

void main(void)
{
  IPV4 a;
  EUI48 mac;
  char *hostname="mega65.org";
  unsigned char i;
  
  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  // Get MAC address from ethernet controller
  for(i=0;i<6;i++) mac_local.b[i] = PEEK(0xD6E9+i);
  
  // Setup WeeIP
  printf("Calling weeip_init()\n");
  weeip_init();
  interrupt_handler();   

  // Do DHCP auto-configuration
  printf("Obtaining IP via DHCP\n");
  dhcp_autoconfig();
  while(!dhcp_configured) {
    task_periodic();
  }
  printf("My IP is %d.%d.%d.%d\n",
	 ip_local.b[0],ip_local.b[1],ip_local.b[2],ip_local.b[3]);
        
#ifdef TEST_TCP_LISTEN   
   printf("Setting up TCP listen on port 55.\n");
   s = socket_create(SOCKET_TCP);
   socket_set_callback(comunica);
   socket_set_rx_buffer(buf, 1024);
   socket_listen(55);
#endif
   
   if (dns_hostname_to_ip(hostname,&a)) {
     printf("Host '%s' resolves to %d.%d.%d.%d\n",
	    hostname,a.b[0],a.b[1],a.b[2],a.b[3]);
   }
   
   printf("Ready for main loop.\n");
   while(1) {
     // XXX Actually only call it periodically
     task_periodic();
   }
}
