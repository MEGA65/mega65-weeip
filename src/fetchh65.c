
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
#include "debug.h"
#include "shared_state.h"

extern char dbg_msg[80];

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

// Names of helper programs
char fetchmdotm65[]={0x46,0x45,0x54,0x43,0x48,0x4d,0x2e,0x4d,0x36,0x35,0};

// Wait for key press before starting
//#define DEBUG_WAIT

#define H65_BEFORE 0
#define H65_TOONEW 1
#define H65_BADADDR 2
#define H65_SENDHTTP 3
#define H65_DONE 255
unsigned char h65_error=0;
unsigned long block_addr,block_len;

unsigned char disconnected=0;
SOCKET *s;
byte_t *buf=(byte_t *)0xC000;

/* Function that is used as a call-back on socket events
 * */
unsigned char last_bytes[4];
int page_parse_state=0;

char http_one_one[]={
  // "HTTP/1.1 ";
  0x48,0x54,0x54,0x50,'/','1','.','1',' ',0};

byte_t comunica (byte_t p)
{
  unsigned int i,count,http_result=0;
   socket_select(s);

   //   printf(".%d(st=%d)",(short)s->rx_data,page_parse_state);
   if ((page_parse_state==0)&&(s->rx_data>12)) {
     for(i=0;i<9;i++) {
       unsigned char c=lpeek(s->rx+i);
       if (c!=http_one_one[i]) break;
     }
     if (i==9) {
       http_result
	 =(lpeek(s->rx+9)-'0')*100
	 +(lpeek(s->rx+10)-'0')*10
	 +(lpeek(s->rx+11)-'0')*1;
       
       fetch_shared_mem.http_result=http_result;
       if (http_result<200||http_result>209) {
	 // Failed to fetch a page due to HTTP error.
	 
	 fetch_shared_mem.job_id++;
	 fetch_shared_mem.state=FETCH_H65FETCH_HTTPERROR;
	 mega65_dos_exechelper(fetchmdotm65);
	 printf("ERROR: Could not load FETCHM.M65\n");
	 while(1) POKE(0xd020,PEEK(0xd020)+1);
	 
       }
     }
   }
   
   switch(p) {
      case WEEIP_EV_CONNECT:
	//	while(1) continue;
	// Buf is setup in fetch_page()

        if (!socket_send(buf, strlen(buf))) 
	  {
	    printf("Error sending HTTP request.\n");
	    h65_error=H65_SENDHTTP;
	  }
	break;
      case WEEIP_EV_DISCONNECT:
	disconnected=1;
         break;
      case WEEIP_EV_DATA:	
	//	while(1) continue;
#ifdef DEBUG_TCP
	snprintf(dbg_msg,80,"   processing %ld received bytes",s->rx_data);
	debug_msg(dbg_msg);
#endif
	for(i=0;i<s->rx_data;i++) {
	  unsigned char c=lpeek(s->rx+i);
	  //	  snprintf(dbg_msg,80,"     %d of %ld",(short)i,(short)s->rx_data);
	  //	  debug_msg(dbg_msg);
	  //	  printf("(%x)",page_parse_state);
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
	      printf("\nFound H65 header.\n");
	      // printf("+");
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
	  case 4: fetch_shared_mem.line_width=c; break; // line width
	  case 5: fetch_shared_mem.d054_bits=c; break; // $D054 bits
	  case 6: fetch_shared_mem.line_display_width=c; break;
	  case 7: fetch_shared_mem.d031_bits=c; break;
	  case 8: fetch_shared_mem.line_count=c; break;
	  case 9: fetch_shared_mem.line_count|=(((unsigned short)c)<<8); break;
	  case 10: fetch_shared_mem.border_colour=c; break;
	  case 11: fetch_shared_mem.screen_colour=c; break;
	  case 12: fetch_shared_mem.text_colour=c; break;
	  case 13: fetch_shared_mem.char_page=c; break;
	  case 14: fetch_shared_mem.d016_bits=c; break;
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
	      printf("\nEnd of page found.\n");
	      page_parse_state=HEADSKIP-1;	      
	      h65_error=H65_DONE;
	      break;
	    } else if (block_addr<0xf000L) {
              printf("bad address $%08lx\n",block_addr);
              h65_error=H65_BADADDR;
              return 0;
            } else if (block_len>0x20000L) {
              printf("bad length $%08lx\n",block_len);
              h65_error=H65_BADADDR;
              return 0;
            } else {
	      // Block data
#if 1
	      POKE(0x286,5);
	      printf("\nBlock addr=$%08lx, len=$%08lx\n\r",
		            block_addr,block_len);
	      POKE(0x286,14);
#endif
	    }
	    break;
	  case HEADSKIP+8:
            // Work out how many bytes we can handle in one go.
            count = s->rx_data - i;
            if (count>block_len) count=block_len;

	    if (count>0) {
	      // Stash them and update it
	      lcopy(s->rx+i,block_addr,count);

#if DEBUG_TCP
	      snprintf(buf,80,"     storing %d data bytes @ $%08lx",count,block_addr);
	      debug_msg(buf);
#endif
	      
	      block_addr+=count;
	      block_len-=count;
	    }

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
	  if (page_parse_state) {
	    page_parse_state++;
	  }
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

void update_mouse_position(void);

void c64_40columns(void)
{
  // Reset video mode to C64 40 column mode while loading
  POKE(0xD054,0);
  POKE(0xD031,0);
  POKE(0xD011,0x1B);
  POKE(0xD016,0xC8);
  POKE(0xD018,0x16);
}

void prepare_network(void)
{
  unsigned char i;

  // Black screen with green text during network setup
  c64_40columns();
  POKE(0xD020,0); POKE(0xD021,0); POKE(0x0286,0x0D);
  printf("%c",0x93);
  
  printf("H65 MAC %02x",mac_local.b[0]);
  for(i=1;i<6;i++) printf(":%02x",mac_local.b[i]);
  
  // Setup WeeIP
  weeip_init();
  task_cancel(eth_task);
  task_add(eth_task, 0, 0,"eth");

  // Do DHCP auto-configuration
  dhcp_configured=0;
  printf("\nRequesting IP...\n");
  dhcp_autoconfig();
  while(!dhcp_configured) {
    task_periodic();
    // Let the mouse move around
    update_mouse_position();
  }
  printf("My IP is %d.%d.%d.%d\n",
	 ip_local.b[0],ip_local.b[1],ip_local.b[2],ip_local.b[3]);
}      

signed long screen_address_offset=0;
signed long screen_address_offset_max=0;

signed long position=0;
signed long max_position=0;

void fetch_page(char *hostname,int port,char *path)
{
  unsigned short i;
  IPV4 a;

  disconnected=0;
  position=0;
  
  POKE(0x0286,0x0e);
  POKE(0xD020,0x0E);
  POKE(0xD021,0x06);
  c64_40columns();
  
restart_fetch:

  printf("%c#%d: H65 Fetching %chttp://%s:%d%s\n",0x93,
	 fetch_shared_mem.job_id,
	 5,hostname,port,path);
  POKE(0x0286,0x0e);

  // NOTE: PETSCII so things are inverted
  snprintf(buf,1024,
	   "get %s http/1.1\n\r"
	   "hOST: %s\n\r"
	   "aCCEPT: */*\n\r"
	   "uSER-aGENT: fetch mega65-wEEip/20220703\n\r"
	   "\n\r",
	   path,hostname);
  // Demunge PETSCII a bit
  for(i=0;buf[i];i++) {
    if (buf[i]>=0xc1) buf[i]-=0x60;
  }
      
  h65_error=0;
  page_parse_state=0;

  // Clear all memory out from last page
  lfill(0x12000L,0,0xD800);
  // Erase screen
  for(i=0;i<24*1024;i+=2) lpoke(0x12000L+i,' ');
  lfill(0xFF82000L,0,0x6000);
  lfill(0x40000L,0,0x0000);  // len=0 means len=64KB
  lfill(0x50000L,0,0x0000);
  
  // Clear any partial match to h65+$ff header
  last_bytes[3]=0;
  
  printf("Resolving %s\n",hostname);
  if (dns_hostname_to_ip(hostname,&a)) {
    printf("Resolved to %d.%d.%d.%d:%d\n",
	   a.b[0],a.b[1],a.b[2],a.b[3],port);
  } else {
    printf("Failed to resolve hostname.\n");

    fetch_shared_mem.job_id++;
    fetch_shared_mem.state=FETCH_H65FETCH_DNSERROR;
    mega65_dos_exechelper(fetchmdotm65);
    printf("ERROR: Could not load FETCHM.M65\n");
    while(1) POKE(0xd020,PEEK(0xd020)+1);
    
  }
  
  s = socket_create(SOCKET_TCP);
  socket_set_callback(comunica);
  // socket_set_rx_buffer(buf, 2048);
  // 128KB of Attic RAM for TCP RX buffer if present
  // XXX - Do a proper auto-detection of the hyperRAM
  socket_set_rx_buffer(0x8000000L, 128*1024);
  socket_connect(&a,port);

  if (disconnected) {
    fetch_shared_mem.job_id++;
    fetch_shared_mem.state=FETCH_H65FETCH_NOCONNECTION;
    mega65_dos_exechelper(fetchmdotm65);
    printf("ERROR: Could not load FETCHM.M65\n");
    while(1) POKE(0xd020,PEEK(0xd020)+1);
  }
  
  while(!disconnected) {
    // XXX Actually only call it periodically

    task_periodic();

    update_mouse_position();
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

	// Return to main program, reporting error
	fetch_shared_mem.job_id++;
	fetch_shared_mem.state=FETCH_H65FETCH_ABORTED;
	
	mega65_dos_exechelper(fetchmdotm65);
	printf("ERROR: Could not load FETCHM.M65\n");
	while(1) POKE(0xd020,PEEK(0xd020)+1);
	
    }
  }

  // Close socket, and call network loop a few times to make sure the FIN ACK gets
  // sent.
  if (h65_error) printf("Error %d occurred.\n",h65_error);
  // XXX -- Launch error handler program if h65_error is non-zero
  printf("Disconnecting...\n");
  socket_disconnect(s);
  for(i=0;i<16;i++) {
    task_periodic();
    if (disconnected) break;
  }
  // And throw away our record of the TCP connection, just to be sure.
  socket_release(s);
  //  printf("Disconnected.\n");
  // Tell main module to display the page
  fetch_shared_mem.job_id++;
  fetch_shared_mem.state=FETCH_H65VIEW;
  mega65_dos_exechelper(fetchmdotm65);
  printf("ERROR: Could not load FETCHM.M65\n");
  while(1) POKE(0xd020,PEEK(0xd020)+1);

}

unsigned char mouse_colours[8]={0x01,0x0a,0x0F,0x0C,0x0B,0x0C,0x0a,0x0F};
// Show active links by mouse pointer colour
unsigned char mouse_link_colours[8]={0x06,0x06,0x0E,0x0E,0x06,0x06,0x0E,0x0E};

unsigned long mouse_link_address=0;
unsigned char link_box[6];

void update_mouse_position(void)
{
  unsigned short mx,my;

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
    mouse_warp_to(mx,50);
    my=50;
  } else if (my>249) {
    mouse_warp_to(mx,249);
    my=249;
  }
  
  {
    
    // Work out mouse position
    // Mouse is in H320, V200, so only 4 mouse pixels per character
    my=((position/2)+my-50)/4;
    mx=(mx-24)/4;

  }
  
  mouse_update_pointer();

}

// Note: These will be interpretted as ASCII by fetch, but CC65 will encode them
// as PETSCII, so text must be upper case here, which will resolve to lower-case
char hostname[256]="192.168.176.31";
char path[256]="/INDEX.H65";
int port=80;

char httpcolonslashslash[8]={0x68,0x74,0x74,0x70,':','/','/',0};
char indexdoth65[11]={'/',0x69,0x6e,0x64,0x65,0x78,'.',0x68,0x36,0x35,0};

void main(void)
{
  // Enable logging of ethernet activity on the serial monitor interface
  //  eth_log_mode=ETH_LOG_TX; // ETH_LOG_RX|ETH_LOG_TX;

  __asm__("sei");
  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  // Give ethernet interface time to auto negotiate etc
  eth_init();

  // Get initial mouse position
  mouse_update_position(NULL,NULL);
  mouse_warp_to(fetch_shared_mem.mouse_x,fetch_shared_mem.mouse_y);
  
  mouse_bind_to_sprite(0);
  mouse_update_pointer();
  mouse_set_bounding_box(24,50-20,320+23,250+20);

#ifdef DEBUG_WAIT
  // Wait for keypress to allow me to connect debugger
  while(!PEEK(0xD610)) continue;
  POKE(0xD610,0);
#endif

  prepare_network();
  
  // Enable sprite 0 as mouse pointer
  POKE(0xD015,0x01);
  // Sprite data from casette buffer
  POKE(0x7F8,0x340/0x40);
  lcopy((unsigned long)&mouse_pointer_sprite,0x340,63);  

  // fetch_page exits to FETCHM by itself, and cannot return here
  lcopy(fetch_shared_mem.host_str_addr,(unsigned short)hostname,256);
  lcopy(fetch_shared_mem.path_str_addr,(unsigned short)path,256);

  fetch_page(hostname,
	     fetch_shared_mem.port,
	     path);

  // Show if something has gone wrong
  while(1) POKE(0xd020,PEEK(0xd020)+1);
  
}
