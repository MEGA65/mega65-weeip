
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
  unsigned int i;
   socket_select(s);
   switch(p) {
      case WEEIP_EV_CONNECT:
	printf("Requesting /home.h65\n");
	// NOTE: PETSCII so things are inverted
	snprintf(buf,1024,
		 "get /HOME.H65 http/1.1\n\r"
		 "hOST: CORES.DEV.MEGA65.ORG\n\r"
		 "aCCEPT: */*\n\r"
		 "uSER-aGENT: mega-browser mega65-weeip/20200907\n\r"
		 "\n\r");
	// Demunge PETSCII a bit
	for(i=0;buf[i];i++) {
	  if (buf[i]>=0xc1) buf[i]-=0x60;
	}
	socket_send(buf, strlen(buf));
	break;
      case WEEIP_EV_DISCONNECT:
         socket_release(s);
         break;
      case WEEIP_EV_DATA:
	printf("Received %d bytes.\n",s->rx_data);
	((char *)s->rx)[s->rx_data]=0;
	printf("%s",s->rx);
	break;
   }

   return 0;
}

void dump_bytes(char *msg,uint8_t *d,int count);

void main(void)
{
  IPV4 a;
  EUI48 mac;
  char *hostname="cores.dev.mega65.org";
  unsigned char i;
  
  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  printf("%c%c",0x05,0x93);
  
  // Get MAC address from ethernet controller
  for(i=0;i<6;i++) mac_local.b[i] = PEEK(0xD6E9+i);
  printf("My MAC address is %02x:%02x:%02x:%02x:%02x:%02x\n",
	 mac_local.b[0],mac_local.b[1],mac_local.b[2],
	 mac_local.b[3],mac_local.b[4],mac_local.b[5]);
  
  // Setup WeeIP
  weeip_init();
  interrupt_handler();   

  // Do DHCP auto-configuration
  printf("Configuring network via DHCP\n");
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

   s = socket_create(SOCKET_TCP);
   socket_set_callback(comunica);
   socket_set_rx_buffer(buf, 1024);
   socket_connect(&a,80);

   
   while(1) {
     // XXX Actually only call it periodically
     task_periodic();
   }
}
