
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
#include "mouse.h"

unsigned char mouse_pointer_sprite[63]={
0xfC,0x00,0x00,
0xf8,0x00,0x00,
0xf0,0x00,0x00,
0xf8,0x00,0x00,
0xdc,0x00,0x00,
0x8e,0x00,0x00,
0x07,0x00,0x00,
0x03,0x80,0x00,
0x01,0xc0,0x00,
0x00,0x80,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00,
0x00,0x00,0x00
};

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

// Wait for key press before starting
//#define DEBUG_WAIT

#define H65_TOONEW 1
#define H65_BADADDR 2
#define H65_SENDHTTP 3
#define H65_DONE 255
unsigned char h65_error=0;
unsigned long block_addr,block_len;
unsigned char d054_bits,d031_bits,line_width,line_display_width,border_colour,screen_colour,text_colour,char_page,d016_bits;
unsigned short line_count;

SOCKET *s;
byte_t *buf=0xC000;

/* Function that is used as a call-back on socket events
 * */
unsigned char last_bytes[4];
int page_parse_state=0;

byte_t comunica (byte_t p)
{
  unsigned int i,count;
   socket_select(s);
   switch(p) {
      case WEEIP_EV_CONNECT:
	printf("Connected...\n");
	// Buf is setup in fetch_page()
        if (!socket_send(buf, strlen(buf))) 
	  {
	    printf("Error sending HTTP request.\n");
	    h65_error=H65_SENDHTTP;
	  }
	break;
      case WEEIP_EV_DISCONNECT:
         break;
      case WEEIP_EV_DATA:
	// printf("Received %d bytes.\n",s->rx_data);
	// Show progress
	printf(".");
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
	      // printf("\nFound H65 header.\n");
	      printf("+");
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
	    // Skip empty block
	    if (block_len==0) {
	      page_parse_state=HEADSKIP-1;
	      h65_error=H65_DONE;
	    } else if (block_addr<0xf000) {
              printf("bad address $%08x\n",block_addr);
              h65_error=H65_BADADDR;
              return 0;
            } else if (block_len>0x20000) {
              printf("bad length $%08x\n",block_len);
              h65_error=H65_BADADDR;
              return 0;
            } else {
	      // Block data
#if 0
	      printf("\nBlock addr=$%08lx, len=$%08lx\n\r",
		            block_addr,block_len);
#endif
	    }
	    break;
	  case HEADSKIP+8:
            // Work out how many bytes we can handle in one go.
            count = s->rx_data - i;
            if (count>block_len) count=block_len;

            // Stash them and update it
            lcopy((unsigned long)&(((char *)s->rx)[i]),block_addr,count);
	    block_addr+=count;
	    block_len-=count;

            // Update i based on the number of bytes digested
            i+=count-1;

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

void update_mouse_position(unsigned char do_scroll);


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
    // Let the mouse move around
    update_mouse_position(0);
  }
  printf("My IP is %d.%d.%d.%d\n",
	 ip_local.b[0],ip_local.b[1],ip_local.b[2],ip_local.b[3]);
}      

signed long screen_address_offset=0;
signed long screen_address_offset_max=0;

signed long position=0;
signed long max_position=0;

void scroll_down(long distance)
{

  // Wait for vertical blank so that we don't have visible tearing.
  while(PEEK(0xD012)<0xf0) continue;
  while(PEEK(0xD012)>=0xf0) continue;


  position+=distance;
  if (position<0) position=0;
  if (position>max_position) position=max_position;

  screen_address_offset=(position/8)*(line_width*2);
  if (screen_address_offset<0) screen_address_offset=0;
  if (screen_address_offset>screen_address_offset_max) screen_address_offset=screen_address_offset_max;  

  if (position&7) {
    // Between chars, so we display part of the previous char
//    screen_address_offset+=line_width*2;
    POKE(0xD04E,0x60+7-(position&7));
  } else {
    // On character boundary
    POKE(0xD04E,0x68);
  }

  // Set screen offset address
  POKE(0xD060,(screen_address_offset>>0));
  POKE(0xD061,(screen_address_offset>>8)+0x20);
  POKE(0xD062,(screen_address_offset>>16)+0x01);
  POKE(0xD063,(screen_address_offset>>24));

  // Set screen offset address
  POKE(0xD064,(screen_address_offset>>0));
  POKE(0xD065,(screen_address_offset>>8)+0x20);
}

void fetch_page(char *hostname,int port,char *path)
{
  unsigned short i;
  IPV4 a;
  unsigned char busy;

restart_fetch:
  
  // Reset video mode to C64 40 column mode while loading
  POKE(0xD054,0);
  POKE(0xD031,0);
  POKE(0xD011,0x1B);
  POKE(0xD016,0xC8);
  POKE(0xD018,0x16);
  POKE(0x0286,0x0e);
  POKE(0xD020,0x0E);
  POKE(0xD021,0x06);
  printf("%cFetching %chttp://%s:%d%s\n",0x93,
	 5,hostname,port,path);
  POKE(0x0286,0x0e);
  
  h65_error=0;
  page_parse_state=0;

  // Clear all memory out from last page
  lfill(0x12000L,0,0xD800);
  lfill(0x40000L,0,0x0000); // 0 means 64KB
  lfill(0x50000L,0,0x0000);
  
  // Clear any partial match to h65+$ff header
  last_bytes[3]=0;
  
  printf("Resolving %s\n",hostname);
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
	   "uSER-aGENT: fetch mega65-weeip/20210722\n\r"
	   "\n\r",
	   path,hostname);
  // Demunge PETSCII a bit
  for(i=0;buf[i];i++) {
    if (buf[i]>=0xc1) buf[i]-=0x60;
  }
    
  s = socket_create(SOCKET_TCP);
  socket_set_callback(comunica);
  socket_set_rx_buffer(buf, 2048);
  socket_connect(&a,port);

  // Erase screen
  for(i=0;i<24*1024;i+=2) lpoke(0x12000L+i,' ');
  while(1) {
    // XXX Actually only call it periodically

    task_periodic();

    update_mouse_position(0);
    if (h65_error) break;
    switch(PEEK(0xD610)) {
      case 0x52: case 0x72:  // Restart fetch
        socket_disconnect(s);
        POKE(0xD610,0);
        goto restart_fetch;
      case 0x03:
        // control-c / RUN/STOP -- abort fetch
        POKE(0xD610,0);
        socket_disconnect(s);
        // XXX Should allow user to enter URL
        return;
    }
  }

  // Close socket, and call network loop until it really is closed
  socket_disconnect(s);
  busy=1;
  while(busy) {
    task_periodic();
    busy=0;  
    for_each(_sockets, _sckt) {
      if(_sckt->type != SOCKET_TCP) continue;               // UDP socket or unused.
      if (_sckt->state!=_IDLE) busy=1;
    }
  }

}

unsigned char mouse_colours[8]={0x01,0x0a,0x0F,0x0C,0x0B,0x0C,0x0a,0x0F};
// Show active links by mouse pointer colour
unsigned char mouse_link_colours[8]={0x06,0x06,0x0E,0x0E,0x06,0x06,0x0E,0x0E};

unsigned long mouse_link_address=0;
unsigned char link_box[6];

void update_mouse_position(unsigned char do_scroll)
{
  unsigned short mx,my,i;

  if (!mouse_link_address) {
    // Cycle the mouse pointer colour
    POKE(0xD027,mouse_colours[(PEEK(0xD7FA)>>2)&0x07]);
  } else {
    POKE(0xD027,mouse_link_colours[(PEEK(0xD7FA)>>2)&0x07]);
  }

  // By default clicking the mouse goes no where
  mouse_link_address=0; 

  mouse_update_position(&mx,&my);
  if (my<50) {
    // Mouse is in top border, so scroll up by that amount
    scroll_down(my-50L);
    mouse_warp_to(mx,50);
    my=50;
  } else if (my>249) {
    // Mouse is in bottom border, so scroll down by that amount
    scroll_down((my-249));
    mouse_warp_to(mx,249);
    my=249;
  }

 {

    // Work out mouse position
    // Mouse is in H320, V200, so only 4 mouse pixels per character
    my=(position+my-50)/4;
    mx=(mx-24)/4;

    // Don't check mouse clicks until scrolling is done  

    if (do_scroll) {
    // Check all bounding boxes
    i=lpeek(0x18000L)+(lpeek(0x18001L)<<8);
    if (i>1000) i=1000;
    while (i>0) {
      i--;
      mouse_link_address=0x18002+6*i;
      lcopy(mouse_link_address,(unsigned long)&link_box[0],6);

      mouse_link_address=0;
      if (link_box[2]>mx) continue;
      if (link_box[3]>my) continue;
      if (link_box[4]<mx) continue;
      if (link_box[5]<my) continue;
      // Get address of URL
      mouse_link_address=0x18000L+link_box[0]+(link_box[1]<<8);
      break;
    } 
  }
  }
  
  mouse_update_pointer();

}

void show_page(void)
{
  printf("h65_error=%d\n",h65_error);

#if 0
  POKE(0x0400,h65_error);
  while(1) {
    POKE(0xD020,PEEK(0xD020)+1);
    if (PEEK(0xD610)) break;
  }
  while (PEEK(0xD610)) POKE(0xD610,0);
#endif

  if (h65_error!=H65_DONE) {
    printf("h65_error=%d\nPress almost any key to continue...\n",h65_error);
    while(!PEEK(0xD610)) continue;
    POKE(0xD610,0);
  } else if (h65_error==H65_DONE) {

    // V400/H640 etc (do first due to hot regs)
    POKE(0xD031,d031_bits);
    // $D016 value
    POKE(0xD016,d016_bits);
    // Enable 16-bit text mode
    POKE(0xD054,0x40+d054_bits);
    // Line step
    POKE(0xD058,line_width*2);
    POKE(0xD059,0);
    // Set screen address to $12000
    POKE(0xD060,0x00);
    POKE(0xD061,0x20);
    POKE(0xD062,0x01);
    POKE(0xD063,0x00);    
    // Set colour RAM address
    POKE(0xD065,0x20);
    // Set charset address
    POKE(0xD069,char_page);
    // Display 51 rows, so that we can do smooth scrolling
    POKE(0xD07B,51-1);
    // Reset smooth scroll (assumes PAL)
    POKE(0xD04E,0x68);

    screen_address_offset_max=0;
    max_position=0;

    if (line_count>50) {
      screen_address_offset_max=(line_width*2)*(line_count-50);
      max_position=(line_count-50)*8;
    }

#if 0
  while(1) {
    POKE(0xD020,PEEK(0xD020)+1);
    if (PEEK(0xD610)) break;
  }
  while (PEEK(0xD610)) POKE(0xD610,0);
#endif

  }
}

  char hostname[64]="192.168.178.31";
  char path[128]="/INDEX.H65";
  char url[256]="";
  int port=8000;

void main(void)
{
  EUI48 mac;
  unsigned char i,hlen,url_ofs;

  // Enable logging of ethernet activity on the serial monitor interface
//  eth_log_mode=ETH_LOG_RX|ETH_LOG_TX;

  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  // Get initial mouse position
  mouse_update_position(NULL,NULL);
  mouse_warp_to(160,100);
  mouse_bind_to_sprite(0);
  mouse_update_pointer();
  mouse_set_bounding_box(24,50-20,320+23,250+20);

#ifdef DEBUG_WAIT
  // Wait for keypress to allow me to connect debugger
  while(!PEEK(0xD610)) continue;
  POKE(0xD610,0);
#endif

  prepare_network();

  // Enable sprite 1 as mouse pointer
  POKE(0xD015,0x01);
  POKE(0xD000,200);
  POKE(0xD001,130);
  // Sprite data from casette buffer
  POKE(0x7F8,0x380/0x40);
  lcopy((unsigned long)&mouse_pointer_sprite,0x380,63);  
  
  fetch_page(hostname,port,path);
  show_page();
  mouse_clicked(); // Clear mouse click status
  mouse_clicked(); // Clear mouse click status
  
  while(1) {
    update_mouse_position(1);

    // Scroll using keyboard
    if (PEEK(0xD610)==0x11) {
      scroll_down(8);
      POKE(0xD610,0);
      } 
    if (PEEK(0xD610)==0x52||PEEK(0xD610)==0x72) {
      // R =  Reload page
      POKE(0xD610,0);
      fetch_page(hostname,port,path);
      show_page();
      } 
    if (PEEK(0xD610)==0x91) {
      scroll_down(-8);
      POKE(0xD610,0);
      }
    if (PEEK(0xD610)==0x70) {
      // Draw rainbow palette
      for(i=0;i<256;i++) {
        lfill(0x40000L+i*64,i,64);
        lpoke(0x12000L+i*2,i&0xff);
        lpoke(0x12000L+i*2+1,0x10+(i>>8));
      }
      POKE(0xD610,0);
      }
    if (mouse_clicked()) {
      if (mouse_link_address) {
        // XXX Don't erase hostname, so that relative paths work automagically.
        // We just set the length to 0, so that if we find a hostname, we can
        // record it correctly. Similarly don't stomp on the port number
        hlen=0;
        url_ofs=0;
        lcopy(mouse_link_address,(unsigned long)&url[0],256);
        if (!strncmp("http://",url,7)) url_ofs=8;
	while(url[url_ofs]!='/'&&url[url_ofs]!=':')
	  { if (hlen<64) { hostname[hlen++]=url[url_ofs]; hostname[hlen]=0; } }
	if (url[url_ofs]==':') {
	  port=0; url_ofs++;
	  while(url[url_ofs]>='0'&&url[url_ofs]<='9') {
	    port*=10; port+=url[url_ofs++]-'0';
	  }
	}
	if (strlen(&url[url_ofs])<128) { strcpy(path,&url[url_ofs]); }
	else { path[0]='/'; path[1]=0; }
	prepare_network();
        fetch_page(hostname,port,path);
      }
    }

    continue;
    }
}
