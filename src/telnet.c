
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

unsigned long byte_log=0;

#define PORT_NUMBER 64128
#define HOST_NAME "byob.hopto.org"
// #define FIXED_DESTINATION_IP

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
byte_t buf[1500];

/* Function that is used as a call-back on socket events
 * */
byte_t comunica (byte_t p)
{
  unsigned int i;
  unsigned char *rx=s->rx;
   socket_select(s);
   switch(p) {
      case WEEIP_EV_CONNECT:
	printf("Connected.\n");
	// Send telnet GO AHEAD command
	socket_send("\0377\0371",2);
	break;
      case WEEIP_EV_DISCONNECT:
         socket_release(s);
         break;
      case WEEIP_EV_DATA:
	// Print what comes from the server
	for(i=0;i<s->rx_data;i++) {
	  lpoke(0x40000+byte_log,rx[i]);
	  byte_log++;
	  //	  if ((rx[i]>=0x20)&&(rx[i]<0x7e)
	  //	      ||(rx[i]==' ')||(rx[i]=='\r')||(rx[i]=='\n'))
	    printf("%c",rx[i]);
	    //	  else
	    //	    printf("[$%02x]",rx[i]);
	}
	lpoke(0x12000,(byte_log>>0)&0xff);
	lpoke(0x12001,(byte_log>>8)&0xff);
	lpoke(0x12002,(byte_log>>16)&0xff);
	lpoke(0x12003,(byte_log>>24)&0xff);
	break;
   }

   return 0;
}

void dump_bytes(char *msg,uint8_t *d,int count);

void main(void)
{
  IPV4 a;
  EUI48 mac;
  unsigned int port_number=PORT_NUMBER;
  char *hostname=HOST_NAME; 

  unsigned char i;
  
  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  POKE(0xD020,0);
  POKE(0xD021,0);
  
  printf("%c%c",0x05,0x93);
  
  // Get MAC address from ethernet controller
  for(i=0;i<6;i++) mac_local.b[i] = PEEK(0xD6E9+i);
  printf("My MAC address is %02x:%02x:%02x:%02x:%02x:%02x\n",
	 mac_local.b[0],mac_local.b[1],mac_local.b[2],
	 mac_local.b[3],mac_local.b[4],mac_local.b[5]);
  
  // Setup WeeIP
  weeip_init();
  interrupt_handler();   

  // Clear buffer of received data we maintain for debugging
  lfill(0x12000,0,4);
  lfill(0x40000,0,32768);
  lfill(0x48000,0,32768);
  lfill(0x50000,0,32768);
  lfill(0x58000,0,32768);
  
  // Do DHCP auto-configuration
  printf("Configuring network via DHCP\n");
  dhcp_autoconfig();
  while(!dhcp_configured) {
    task_periodic();
  }

#ifdef FIXED_DESTINATION_IP
     a.b[0]=192;
     a.b[1]=168;
     a.b[2]=178;
     a.b[3]=31;
#else  
  printf("My IP is %d.%d.%d.%d\n",
	 ip_local.b[0],ip_local.b[1],ip_local.b[2],ip_local.b[3]);
        
   if (!dns_hostname_to_ip(hostname,&a)) {
     printf("Could not resolve hostname '%s'\n",hostname);
   } else {
   }
#endif
   
   printf("Host '%s' resolves to %d.%d.%d.%d\n",
	  hostname,a.b[0],a.b[1],a.b[2],a.b[3]);
   
   s = socket_create(SOCKET_TCP);
   socket_set_callback(comunica);
   socket_set_rx_buffer(buf, 1500);
   socket_connect(&a,port_number);

   // Text to light green by default
   POKE(0x0286,0x0d);
   
   while(1) {
     // XXX Actually only call it periodically
     task_periodic();

     if (PEEK(0xD610)) {
       if (PEEK(0xD610)==0xF9) {
	 printf("%c%c%c%c%c%cDisconnecting...",0x0d,0x05,0x12,0x11,0x11,0x11,0x11);
	 socket_disconnect();
       }
       
       buf[0]=PEEK(0xD610);
       POKE(0xD610,0);
       socket_send(buf, 1);
       POKE(0xD020,PEEK(0xD020)+1);
     }
   }
}
