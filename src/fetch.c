
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

#define H65_TOONEW 1
#define H65_DONE 255
unsigned char h65_error=0;
unsigned long block_addr,block_len;
unsigned char d054_bits,d031_bits,line_width,line_display_width,border_colour,screen_colour,text_colour,char_page,d016_bits;
unsigned short line_count;

SOCKET *s;
byte_t buf[1024];

/* Function that is used as a call-back on socket events
 * */
unsigned char last_bytes[4];
int page_parse_state=0;

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
	//	printf("Received %d bytes.\n",s->rx_data);
	for(i=0;i<s->rx_data;i++) {
	  unsigned char c=((char *)s->rx)[i];
	  //	  printf("(%d)",page_parse_state);
	  switch(page_parse_state) {
	  case 0:
	    // Look for H65+$FF header
	    last_bytes[0]=last_bytes[1];
	    last_bytes[1]=last_bytes[2];
	    last_bytes[2]=last_bytes[3];
	    last_bytes[3]=c;
	    if (last_bytes[0]==0x48
		&&last_bytes[1]==0x36
		&&last_bytes[2]==0x35
		&&last_bytes[3]==0xFF) {
	      page_parse_state=2-1; // gets incremented below
	      printf("Found H65 header.\n");
	    }
	    break;
	  case 2:
	    // Reading H65 header fields
	    if (c!=1) {
	      // Unsupported H65 version
	      h65_error=H65_TOONEW;
	    }
	    break;
	  case 3: break; // Ignore minor version
	  case 4: line_width=c; break; // line width
	  case 5: d054_bits=c; break; // $D054 bits
	  case 6: line_display_width=c; break;
	  case 7: d031_bits=c; break;
	  case 8: line_count=c; break;
	  case 9: line_count|=(((unsigned short)c)<<8); break;
	  case 10: border_colour=c; break;
	  case 11: screen_colour=c; break;
	  case 12: text_colour=c; break;
	  case 13: char_page=c; break;
	  case 14: d016_bits=c; break;
	    // Block header: Address
#define HEADSKIP 126
	  case HEADSKIP+0: ((char *)&block_addr)[0]=c; break;
	  case HEADSKIP+1: ((char *)&block_addr)[1]=c; break;
	  case HEADSKIP+2: ((char *)&block_addr)[2]=c; break;
	  case HEADSKIP+3: ((char *)&block_addr)[3]=c; break;
	    // Block header: Length
	  case HEADSKIP+4: ((char *)&block_len)[0]=c; break;
	  case HEADSKIP+5: ((char *)&block_len)[1]=c; break;
	  case HEADSKIP+6: ((char *)&block_len)[2]=c; break;
	  case HEADSKIP+7: ((char *)&block_len)[3]=c;
	    // Skip empty blocks
	    if (block_len==0) {
	      page_parse_state=HEADSKIP-1;
	      h65_error=H65_DONE;
	    } else {
	      // Block data
	      printf("Block addr=$%08lx, len=$%08lx\n\r",
		     block_addr,block_len);
	    }
	    break;
	  case HEADSKIP+8:
	    lpoke(block_addr,c);
	    block_addr++;
	    block_len--;
	    // Read next block
	    if (!block_len) page_parse_state=HEADSKIP-1;
	    else page_parse_state=HEADSKIP+8-1;
            break;
	  default:
	    // ???
	    break;
	  }
	  if (page_parse_state) page_parse_state++;
#if 0
          POKE(0x427,c);
	  while(!PEEK(0xD610)) continue; POKE(0xD610,0);
#endif
	}
//	((char *)s->rx)[s->rx_data]=0;
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

  h65_error=0;

  // Clear any partial match to h65+$ff header
  last_bytes[3]=0;
  
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

  // Erase screen and colour RAM
lfill(0x12000L,0x00,0x6000);
 for(i=0;i<24*1024;i+=2) lpoke(0x12000L+i,' ');
lfill(0xFF80000L,0x00,0x6000);
  
  while(1) {
    // XXX Actually only call it periodically
    task_periodic();
    if (h65_error) break;
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

  if (h65_error==H65_DONE) {
    // V400/H640 etc (do first due to hot regs)
    POKE(0xD031,d031_bits);
    // $D016 value
    POKE(0xD016,d016_bits);
    // Enable 16-bit text mode
    POKE(0xD054,0x40+d054_bits);
    // Line step
    POKE(0xD058,line_width*2);
    POKE(0xD059,0);
    // Set screen address
    POKE(0xD060,0x00);
    POKE(0xD061,0x20);
    POKE(0xD062,0x01);
    POKE(0xD063,0x00);
    // Set colour RAM address
    POKE(0xD065,0x20);
    // Set charset address
    POKE(0xD069,char_page);
  }
  
  while(1) continue;
}
