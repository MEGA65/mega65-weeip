
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
	printf("Connected...\n");
	// Buf is setup in fetch_page()
	socket_send(buf, strlen(buf));
	break;
      case WEEIP_EV_DISCONNECT:
         socket_release(s);
         break;
      case WEEIP_EV_DATA:
	printf("Received %d bytes.\n",s->rx_data);
	((char *)s->rx)[s->rx_data]=0;
	//	printf("%s",s->rx);
	break;
   }

   return 0;
}

void dump_bytes(char *msg,uint8_t *d,int count);

void prepare_network(void)
{
  unsigned char i;

  // Black screen with green text during network setup
  POKE(0xD020,0); POKE(0xD021,0); POKE(0x0286,0x0D);
  printf("%c",0x93);
  
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
}      

void setup_screen80(void)
{
}

void fetch_page(char *hostname,int port,char *path)
{
  unsigned short i;
  IPV4 a;

  // Revert to C64 40 column display and colours
  // and show progress of fetching the page
  POKE(0xD020,0x0e);
  POKE(0xD021,0x06);
  POKE(0x0286,0x0e);
  printf("%c",0x93);

  printf("Resolving hostname %s\n",hostname);
  if (dns_hostname_to_ip(hostname,&a)) {
    printf("Resolved to %d.%d.%d.%d\n",
	   a.b[0],a.b[1],a.b[2],a.b[3]);
  } else {
    printf("Failed to resolve hostname.\n");
    return;
  }
  

  // NOTE: PETSCII so things are inverted
  snprintf(buf,1024,
	   "get %s http/1.1\n\r"
	   "hOST: %s\n\r"
	   "aCCEPT: */*\n\r"
	   "uSER-aGENT: mega-browser mega65-weeip/20210722\n\r"
	   "\n\r",
	   path,hostname);
  // Demunge PETSCII a bit
  for(i=0;buf[i];i++) {
    if (buf[i]>=0xc1) buf[i]-=0x60;
  }
    
  s = socket_create(SOCKET_TCP);
  socket_set_callback(comunica);
  socket_set_rx_buffer(buf, 1024);
  socket_connect(&a,port);
    
  while(1) {
    // XXX Actually only call it periodically
    task_periodic();
  }
}

void main(void)
{
  EUI48 mac;
  char *hostname="192.168.178.31";
  int port=8000;
  unsigned char i;
  
  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  prepare_network();

  fetch_page(hostname,port,"/TEST.H65");

}
