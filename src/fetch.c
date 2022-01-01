
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

// Wait for key press before starting
//#define DEBUG_WAIT

#define H65_BEFORE 0
#define H65_TOONEW 1
#define H65_BADADDR 2
#define H65_SENDHTTP 3
#define H65_DONE 255
unsigned char h65_error=0;
unsigned long block_addr,block_len;
unsigned char d054_bits,d031_bits,line_width,line_display_width,border_colour,screen_colour,text_colour,char_page,d016_bits;
unsigned short line_count;

unsigned char disconnected=0;
SOCKET *s;
byte_t *buf=(byte_t *)0xC000;

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
	// Show progress
	printf(".%d",s->rx_data);
	//	while(1) continue;
	for(i=0;i<s->rx_data;i++) {
	  unsigned char c=lpeek(s->rx+i);
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
	      //	      printf("\nDONE!\n");
	      page_parse_state=HEADSKIP-1;	      
	      h65_error=H65_DONE;
	      break;
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

#if 0
	      snprintf(buf,80,"%d @ $%08lx",count,block_addr);
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

void update_mouse_position(unsigned char do_scroll);

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
  
  printf("MAC %02x",mac_local.b[0]);
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

  // Set screen colour RAM offset address
  POKE(0xD064,(screen_address_offset>>0));
  POKE(0xD065,(screen_address_offset>>8)+0x20);
}

void fetch_page(char *hostname,int port,char *path)
{
  unsigned short i;
  IPV4 a;
  unsigned char busy;

  disconnected=0;
  position=0;
  
  POKE(0x0286,0x0e);
  POKE(0xD020,0x0E);
  POKE(0xD021,0x06);
  c64_40columns();
  
restart_fetch:

  printf("%cFetching %chttp://%s:%d%s\n",0x93,
	 5,hostname,port,path);
  POKE(0x0286,0x0e);

  // NOTE: PETSCII so things are inverted
  snprintf(buf,1024,
	   "get %s http/1.1\n\r"
	   "hOST: %s\n\r"
	   "aCCEPT: */*\n\r"
	   "uSER-aGENT: fetch mega65-weeip/20210727\n\r"
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
    return;
  }
  
  s = socket_create(SOCKET_TCP);
  socket_set_callback(comunica);
  // socket_set_rx_buffer(buf, 2048);
  // 128KB of Attic RAM for TCP RX buffer if present
  socket_set_rx_buffer(0x8000000L, 128*1024);
  socket_connect(&a,port);

  while(!disconnected) {
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

  // Close socket, and call network loop a few times to make sure the FIN ACK gets
  // sent.
  printf("Disconnecting... %d\n",h65_error);
  socket_disconnect(s);
  for(i=0;0<16;i++) {
    task_periodic();
    if (disconnected) break;
  }
  // And throw away our record of the TCP connection, just to be sure.
  socket_release(s);
  printf("Disconnected.\n");

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
    if (do_scroll) scroll_down(my-50L);
    mouse_warp_to(mx,50);
    my=50;
  } else if (my>249) {
    // Mouse is in bottom border, so scroll down by that amount
    if (do_scroll) scroll_down((my-249));
    mouse_warp_to(mx,249);
    my=249;
  }
  
  {
    
    // Work out mouse position
    // Mouse is in H320, V200, so only 4 mouse pixels per character
    my=((position/2)+my-50)/4;
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
  //  while(!PEEK(0xD610)) POKE(0xD020,PEEK(0xD020)+1); POKE(0xD610,0);
  
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
    // Show mouse pointer
    POKE(0xD015,0x01);
    
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

// Note: These will be interpretted as ASCII by fetch, but CC65 will encode them
// as PETSCII, so text must be upper case here, which will resolve to lower-case
char hostname[64]="192.168.176.31";
char path[128]="/INDEX.H65";
int port=80;

char httpcolonslashslash[8]={0x68,0x74,0x74,0x70,':','/','/',0};
char indexdoth65[11]={'/',0x69,0x6e,0x64,0x65,0x78,'.',0x68,0x36,0x35,0};

void parse_url(unsigned long addr)
{
  unsigned char hlen,url_ofs,plen;

  // NOTE: We are using the TCP receive buffer for the URL
  // since there should be no need to parse URLs, while there
  // is outstanding TCP RX data, since we load pages entirely
  // before allowing clicking on links etc
  lcopy(addr,(unsigned long)&buf[0],256);  

  printf("Parsing '%s'\n",buf);
  
  // Update browsing history
  // By checking the string length like this, we rather magically
  // don't add pages to the history when we are going back through
  // the browser history.
  if (buf[0]&&(strlen(buf)<160)) {
    // Rotate history slots down
    for(hlen=11;hlen>1;hlen--) {
      lcopy(0xD000+80-160+hlen*160,0xD000+80+hlen*160,160);
    }
    // the URL from the 2nd line of the screen (this is how the
    // G = goto url key works.
    lfill(0xD000+80*3,0x20,160);
    if (buf[0]=='/') {
      // Put http://host:port on front of paths
      snprintf(&buf[256],256,"%s%s:%d%s",httpcolonslashslash,hostname,port,buf);
      lcopy((unsigned long)&buf[256],0xD000+80*3,strlen(&buf[256]));
    } else
      // Otherwise leave path untouched
      lcopy(addr,0xD000+80*3,strlen(buf));
  }
  
  hlen=0;
  url_ofs=0;
  plen=0;
  if (!strncmp(httpcolonslashslash,buf,7)) url_ofs=7;
  while(buf[url_ofs]!=' '&&buf[url_ofs]&&buf[url_ofs]!='/'&&buf[url_ofs]!=':')
    {
      if (hlen<64) { hostname[hlen++]=buf[url_ofs++]; hostname[hlen]=0; }
      else break;
    }
  if (hlen) port=80;
  if (buf[url_ofs]==':') {
    port=0; url_ofs++;
    while(buf[url_ofs]>='0'&&buf[url_ofs]<='9') {
      port*=10; port+=buf[url_ofs++]-'0';
    }
  }
  while(buf[url_ofs]&&buf[url_ofs]!=' ') {
    if (plen<127) path[plen++]=buf[url_ofs++];
  }
  path[plen]=0;
  // If no path, then we use /index.h65
  if (!plen) { strcpy(path,indexdoth65); }
  else if (path[plen-1]=='/'&&(plen<100)) {
    // URL ends in /. User probably needs a filename on the end
    strcpy(&path[plen-1],indexdoth65);
  }  

}

void select_url(void)
{
  unsigned char c;
  unsigned char url_ofs;
  unsigned char line_num=0;
  
  // H640,V200, VIC-III attributes
  POKE(0xD031,0xa0);
  // $D016 value
  POKE(0xD016,0xC9);
  // Enable 8-bit text mode
  POKE(0xD054,0x40);
  // Line step
  POKE(0xD058,80);
  POKE(0xD059,0);
  // Set screen address to $D000
  POKE(0xD060,0x00);
  POKE(0xD061,0xD0);
  POKE(0xD062,0x00);
  POKE(0xD063,0x00);    
  // Set colour RAM offset to $1000
  POKE(0xD065,0x10);
  // Set charset address
  // XXX Assumes ASCII font loaded by web page at $F000-$F7FF (!!)
  POKE(0xD069,0xf0);
  // Display 25 rows, so that we can do smooth scrolling
  POKE(0xD07B,25-1);
  // Reset smooth scroll (assumes PAL)
  POKE(0xD04E,0x68);

  // Hide mouse pointer, since we don't support it here
  POKE(0xD015,0);
  
  position=0; max_position=0;
  screen_address_offset_max=0;

  // Reset screen colour
  lfill(0xFF81000L,0x0e,2000);
  // Underline and reset colour of heading
  lfill(0xff81000,0x87,19);

  while(1) {

    // Make sure we re-draw during vertical blank, so that we don't
    // see any tearing
    while(!(PEEK(0xD011)&0x80)) continue;
    while(PEEK(0xD012)<0x10||PEEK(0xD012)>0x20) continue;

    if (line_num) {
      // A previous URL is selected, so highlight it
      lfill(0xFF81000+80,0x0e,2000-80);
      lfill(0xFF81000+80+line_num*160,0x21,80*2);
    } else {
      // Highlight URL line being typed into
      lfill(0xff81000+80,0x21,80*2);
      // Reset text colour of remainder of screen
      lfill(0xFF81000+80+160,0x0e,2000-80*3);
    }
    lpoke(0xFF81000L+80+url_ofs,line_num?0x3e:0x31);    
    
    // Draw/undraw hardware cursor
    c=PEEK(0xD610); POKE(0xd610,0);
    if (!c) continue;
    else lpoke(0xFF81000L+80+url_ofs,0x01);
    
    switch(c) {
    case 0x11: // down
      line_num++;
      break;
    case 0x91: // up
      line_num--;
      break;
    case 0x1d: // right
      line_num=0;
      if (url_ofs<159) url_ofs++;
      break;
    case 0x9d: // left
      line_num=0;
      if (url_ofs) url_ofs--;
      break;
    case 0x0d: // return
      if (line_num) {
	// Copy previous URL and allow editing it
	lcopy(0xD000+80+line_num*160,0xD000+80,160);
	line_num=0; url_ofs=159;
	// Place cursor at the end of the URL
	while(url_ofs&&lpeek(0xD000+80+url_ofs)==' ') url_ofs--;
	if (url_ofs<159) url_ofs++;
      } else {
	// return on editing URL tries to load it.
	POKE(0xD020,0);
	
	// But first, null terminate it.
	c=159;
	while(c&&(lpeek(0xD000+80+c)==0x20)) {
	  lpoke(0xD000+80+c,0);
	  c--;
	}
	// But only return the URL if it is blank
	if (c) return;
	else
	  // Reverse the null termination so that we don't do silly things
	  lfill(0xD000+80,0x20,160);
      }
      break;
    case 0x14: // back space
      line_num=0;
      if (url_ofs&&url_ofs<160) {
	lcopy(0xD000+80+url_ofs,0xD000+79+url_ofs,160-url_ofs);
      }
      lpoke(0xD000+80+159,' ');
      if (url_ofs) url_ofs--;
      break;
    case 0x94: // insert
      line_num=0;
      if (url_ofs<158) {
	lcopy(0xD000+80+url_ofs,0xD000+81+url_ofs,158-url_ofs);
      }
      lpoke(0xD000+80+url_ofs,' ');
      break;
    default:
      line_num=0;
      lpoke(0xD000+80+url_ofs,c);
      if (url_ofs<159) url_ofs++;
      break;
    }
    // Loop around the line number selection
    if (line_num>200) line_num=11;
    else if (line_num>11) line_num=0;
  }
}

unsigned char type_url[19]=
  {0x54,0x79,0x70,0x65,0x20, // Type
   0x6f,0x72,0x20, // or
   0x73,0x65,0x6c,0x65,0x63,0x74,0x20, // select
   0x55,0x52,0x4c, // URL
   0x3a // :
  };

void main(void)
{
  unsigned char i,reload;

  // Enable logging of ethernet activity on the serial monitor interface
  //  eth_log_mode=ETH_LOG_TX; // ETH_LOG_RX|ETH_LOG_TX;

  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  // Clear out URL history area
  lfill(0xD000L,0x20,4096);
  lcopy((unsigned long)type_url,0xD000L,19);
  
  // Give ethernet interface time to auto negotiate etc
  eth_init();

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
  
  // Enable sprite 0 as mouse pointer
  POKE(0xD015,0x01);
  // Sprite data from casette buffer
  POKE(0x7F8,0x380/0x40);
  lcopy((unsigned long)&mouse_pointer_sprite,0x380,63);  

  // Get initial URL, and display it.
  // This allows having preset URLs if we pre-load
  // the RAM at $Dxxx with the bookmarks
  // If loading the page fails, then it will get caught
  // in the main loop, because m65_error will flag it.

  select_url();
  parse_url(0xD000 + 80);

  fetch_page(hostname,port,path);
  show_page();
  
  mouse_clicked(); // Clear mouse click status
  mouse_clicked(); // Clear mouse click status
  
  while(1) {
    reload=0;
    update_mouse_position(1);

    // Check for keyboard input
    i=PEEK(0xD610); POKE(0xD610,0);
    switch(i) {
    case 0x5f:
      // Navigate back, but only if we have a previous
      // page to load
      if (lpeek(0xD000 + 5*80)!=0x20) {
	// Copy all history entries up one slot
	lcopy(0xD000 + 5*80, 0xD000 + 3*80,80*20);
	// Clear the bottom slot, now that it is empty
	lfill(0xD000 + 24*80,0x20,2*80);
	// Request loading of the previous URL
	lcopy(0xD000 + 3*80,0xe000,160);
	parse_url(0xD000 + 3*80);
	reload=1;
      }
      break;
    case 0x11:
      // Cursor down = scroll page down one line
      scroll_down(8);
      break;
    case 0x67:
      // G = Goto URL
      select_url();
      parse_url(0xD000 + 80);
      reload=1;
      break;
    case 0x72:
      // R =  Reload page
      reload=1;
      break;
    case 0x91:
      // Cursor up == scroll page up one line
      scroll_down(-8);
      break;
    }
    if (mouse_clicked()) {
      if (mouse_link_address) {
        // XXX Don't erase hostname, so that relative paths work automagically.
        // We just set the length to 0, so that if we find a hostname, we can
        // record it correctly. Similarly don't stomp on the port number
	reload=1;
	parse_url(mouse_link_address);
      }
    }

    // Reload page if requested, and keep trying if page load fails for some reason.
    if (reload||h65_error!=H65_DONE) {
      h65_error=H65_BEFORE;
      while(h65_error!=H65_DONE) {
	fetch_page(hostname,port,path);
	show_page();
	if (h65_error!=H65_DONE) select_url();
      }
    }
  }

}
