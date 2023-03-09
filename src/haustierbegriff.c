
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

unsigned char last_frame_number=0;

unsigned long byte_log=0;

#define PORT_NUMBER 64128
#define HOST_NAME "rapidfire.hopto.org"
//#define FIXED_DESTINATION_IP

struct bbs {
  char *name;
  char *host_name;
  unsigned int port_number;
};

const struct bbs bbs_list[27]=
  {
   {"Boar's Head","byob.hopto.org",64128},
   {"RapidFire","rapidfire.hopto.org",64128},
   {"Antidote by Triad","antidote.hopto.org",64128},
   {"Wizards's Realm", "wizardsrealm.c64bbs.org", 23},
   {"The Hidden", "the-hidden.hopto.org", 64128},
   {"Eaglewing BBS", "eagelbird.ddns.net", 6400},
   {"Scorps Portal", "scorp.us.to", 23},
   {"My C=ult BBS", "maraud.dynalias.com", 6400},
   {"Commodore Image", "cib.dyndns.org", 6400},
   {"64 Vintag Remic", "64vintageremixbbs.dyndns.org", 6400},
   {"Jamming Signal", "bbs.jammingsignal.com", 23},
   {"Centronian BBS", "centronian.servebeer.com", 6400},
   {"Anrchy Undergrnd", "aubbs.dyndns.org", 2300},
   {"The Oasis BBS", "oasisbbs.hopto.org", 6400},
   {"The Disk Box", "bbs.thediskbox.com", 6400},
   {"Cottonwood", "cottonwoodbbs.dyndns.org", 6502},
   {"Wrong Number ][", "cib.dyndns.org", 6404},
   {"RabidFire", "rapidfire.hopto.org", 64128},
   {"Mad World", "madworld.bounceme.net", 6400},
   {"Citadel 64", "bbs.thejlab.com", 6400},
   {"Hotwire BBS", "hotwirebbs.zapto.org", 6502},
   {"Endless Chaos", "endlesschaos.dyndns.org", 6400},
   {"Borderline", "borderlinebbs.dyndns.org", 6400},
   {"RAVELOUTION","raveolution.hopto.org",64128},
   {"The Edge BBS","theedgebbs.dyndns.org",1541},
   {"PGS Test","F96NG92-L.fritz.box",64128},
   {NULL,NULL,0}
  };



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
  case WEEIP_EV_DATA:
  case WEEIP_EV_DISCONNECT_WITH_DATA:
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
    // Fall through if its a disconnect with data
    if (p==WEEIP_EV_DATA) break;
    // FALL THROUGH
  case WEEIP_EV_DISCONNECT:
    socket_release(s);
    printf("%c%c\nDISCONNECTED\n",5,12);
    break;
  }
  
  return 0;
}

void dump_bytes(char *msg,uint8_t *d,int count);

unsigned char nbbs=0;

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

  // Clear $D610 key buffer
  while(PEEK(0xD610)) POKE(0xD610,0);
  
  // Fix invalid MAC address multicast bit
  POKE(0xD6E9,PEEK(0xD6E9)&0xFE);
  // Mark MAC address as locally allocated
  POKE(0xD6E9,PEEK(0xD6E9)|0x02);
  
  // Get MAC address from ethernet controller
  for(i=0;i<6;i++) mac_local.b[i] = PEEK(0xD6E9+i);
  printf("My MAC address is %02x:%02x:%02x:%02x:%02x:%02x\n",
	 mac_local.b[0],mac_local.b[1],mac_local.b[2],
	 mac_local.b[3],mac_local.b[4],mac_local.b[5]);
  
  // Setup WeeIP
  printf("Resetting ethernet controller\n");
  weeip_init();
  task_cancel(eth_task);
  task_add(eth_task, 0, 0,"eth");
  
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
    POKE(0x0400+999,PEEK(0x0400+999)+1);
  }

#ifdef FIXED_DESTINATION_IP
     a.b[0]=192;
     a.b[1]=168;
     a.b[2]=178;
     a.b[3]=31;
#else  
  printf("My IP is %d.%d.%d.%d\n",
	 ip_local.b[0],ip_local.b[1],ip_local.b[2],ip_local.b[3]);

  printf("Please select a BBS:\n");
  for(nbbs=0;bbs_list[nbbs].port_number;nbbs++) {
    printf("%c.%-17s ",'a'+nbbs,bbs_list[nbbs].name);
  }
  printf("\n");
  
  while(1) {
    if (PEEK(0xD610)) {
      if ((PEEK(0xD610)>=0x61)&&(PEEK(0xD610)-0x61)<nbbs) {
	nbbs=PEEK(0xD610)-0x61;
	hostname=bbs_list[nbbs].host_name;
	port_number=bbs_list[nbbs].port_number;
	POKE(0xD610,0);
	break;
      } else POKE(0xD610,0);
    }
    task_periodic();
  }
  POKE(198,0);
  printf("Preparing to connect to %s\n",bbs_list[nbbs].name);
  
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
     if (PEEK(0xD7FA)!=last_frame_number) {
       task_periodic();
       last_frame_number=PEEK(0xD7FA);
     }

     // Monitor hardware accelerated keyboard input for extra C65 keys only
     if (PEEK(0xD610)) {
       if (PEEK(0xD610)==0xF9) {
	 printf("%c%c%c%c%c%cDisconnecting...",0x0d,0x05,0x12,0x11,0x11,0x11,0x11);
	 socket_reset();
       }
       POKE(0xD610,0);
     }

#if 1
     // Directly read from C64's keyboard buffer
     if (PEEK(198)) {
       lcopy(631,(long)buf,10);
       socket_select(s);
       // Only consume keys if socket_send() succeeds
       if (socket_send(buf, PEEK(198))) {
	 POKE(198,0);
       }
     }
#endif     
   }
}
