
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "defs.h"
#include "mega65/random.h"
#include "mega65/memory.h"
#include "mega65/mouse.h"
#include "mega65/debug.h"

#include "weeip.h"
#include "checksum.h"
#include "eth.h"
#include "arp.h"

#include "h65.h"
#include "shared_state.h"

void interact_page(void);

unsigned char j1=0;
unsigned char last_j1=0;

// Wait for key press before starting
//#define DEBUG_WAIT

unsigned char h65_error=0;
unsigned long block_addr,block_len;

void update_mouse_position(unsigned char do_scroll);

#define GRAZE_MOUSE_POINTERS
#define GRAZE_C64_40COLUMNS
#include "../src/graze_common.c"

char grazeh65[]={0x47,0x52,0x41,0x5A,0x45,0x48,'6','5','.',0x4d,'6','5',0};
char grazeget[]={0x47,0x52,0x41,0x5A,0x45,0x47,0x45,0x54,'.',0x4d,'6','5',0};

// Note: These will be interpretted as ASCII by fetch, but CC65 will encode them
// as PETSCII, so text must be upper case here, which will resolve to lower-case
#define HOSTNAME_LEN 64
#define PATH_LEN 128
char hostname[HOSTNAME_LEN]="192.168.176.31";
char path[PATH_LEN]="/INDEX.H65";
int port=80;

char httpcolonslashslash[8]={0x68,0x74,0x74,0x70,':','/','/',0};
char indexdoth65[11]={'/',0x69,0x6e,0x64,0x65,0x78,'.',0x68,0x36,0x35,0};

char buf[256+1];
char tempurl[256+1];

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

  screen_address_offset=(position/8)*(graze_shared_mem.line_width*2);
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
  /*
    Call GRAZEH65 for H65 pages, or GRAZEGET for downloading files.

  */

  char *ext;

  // Show hourglass while loading new page
  POKE(0xD027,0x01); // make hourglass white
  lcopy((unsigned long)&mouse_hourglass_sprite,0x340,63);
  
  // Setup URL port and strings at $F800 and $F900
  // for shared interaction with other modules
  lcopy((unsigned short)hostname,0xf800,HOSTNAME_LEN);
  lcopy((unsigned short)path,0xf900,PATH_LEN);
  graze_shared_mem.host_str_addr=0xf800;
  graze_shared_mem.path_str_addr=0xf900;
  graze_shared_mem.port=port;

  graze_shared_mem.job_id++;
  
  // Find . that separates extension
  ext=&path[strlen(path)-1];  
  while (ext>path&&ext[-1]!='.') ext--;

  // CC65 PETSCII / ASCII mixing is a real pain, so we have to compare char by char here
  if (ext[1]=='6'&&ext[2]=='5'&&((ext[0]&0xdf)==0x48)) {
    // Fetch H65 page
    mega65_dos_exechelper(grazeh65);
    printf("ERROR: Could not load GRAZEH65.M65\n");
    while(1) POKE(0xd020,PEEK(0xd020)+1);
    
  } else {
    // Download file
    // XXX - We should check for .HTML, .PHP and other common extension for "normal" web pages,
    // and try to display them. But that will be done in the FETCH_GETFETCH_DOWNLOADED state
    // in main.  For now, we just need to call GRAZEGET.M65

    mega65_dos_exechelper(grazeget);
    printf("ERROR: Could not load GRAZEGET.M65\n");
    while(1) POKE(0xd020,PEEK(0xd020)+1);    
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
    
    // Work out where the mouse position is in the page
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
  
#if 0
  while(1) {
    POKE(0xD020,PEEK(0xD020)+1);
    if (PEEK(0xD610)) break;
  }
  while (PEEK(0xD610)) POKE(0xD610,0);
#endif

  // Show mouse pointer
  POKE(0xD015,0x01);
  
  // V400/H640 etc (do first due to hot regs)
  POKE(0xD031,graze_shared_mem.d031_bits);
  // $D016 value
  POKE(0xD016,graze_shared_mem.d016_bits);
  // Enable 16-bit text mode
  POKE(0xD054,0x40+graze_shared_mem.d054_bits);
  // Line step
  POKE(0xD058,graze_shared_mem.line_width*2);
  POKE(0xD059,0);
  // Set screen address to $12000
  POKE(0xD060,0x00);
  POKE(0xD061,0x20);
  POKE(0xD062,0x01);
  POKE(0xD063,0x00);    
  // Set colour RAM address
  POKE(0xD065,0x20);
  // Set charset address
  POKE(0xD069,graze_shared_mem.char_page);
  // Display 51 rows, so that we can do smooth scrolling
  POKE(0xD07B,51-1);
  // Reset smooth scroll (assumes PAL)
  POKE(0xD04E,0x68);
  
  screen_address_offset_max=0;
  max_position=0;
  
  if (graze_shared_mem.line_count>50) {
    screen_address_offset_max=(graze_shared_mem.line_width*2)*(graze_shared_mem.line_count-50);
    max_position=(graze_shared_mem.line_count-50)*8;
  }
  
#if 0
  while(1) {
    POKE(0xD020,PEEK(0xD020)+1);
    if (PEEK(0xD610)) break;
  }
  while (PEEK(0xD610)) POKE(0xD610,0);
#endif

  // Finally set screen and border colours
  POKE(0xD020,graze_shared_mem.border_colour);
  POKE(0xD021,graze_shared_mem.screen_colour);
  
}

void parse_url(unsigned long addr)
{
  unsigned char hlen,url_ofs,plen;

  lcopy(addr,(unsigned long)&buf[0],256);  

  printf("Parsing URL '%s'\n",buf);
  
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
      snprintf(tempurl,sizeof(tempurl),"%s%s:%d%s",httpcolonslashslash,hostname,port,buf);
      lcopy((unsigned long)tempurl,0xD000+80*3,strlen(tempurl));
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
  unsigned char url_ofs=0;
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

#ifdef LLVM
int 
#else
void
#endif
main(void)
{
  // Enable logging of ethernet activity on the serial monitor interface
  //  eth_log_mode=ETH_LOG_TX; // ETH_LOG_RX|ETH_LOG_TX;

  __asm__("sei");
  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  // Restore mouse position from IPC
  mouse_update_position(NULL,NULL);
  mouse_warp_to(graze_shared_mem.mouse_x,graze_shared_mem.mouse_y);
  mouse_bind_to_sprite(0);
  mouse_update_pointer();
  mouse_set_bounding_box(24,50-20,320+23,250+20);
  mouse_clicked(); // Clear mouse click status
  mouse_clicked(); // Clear mouse click status

  // Enable sprite 0 as mouse pointer
  POKE(0xD015,0x01);
  // Sprite data from casette buffer
  POKE(0x7F8,0x340/0x40);
  lcopy((unsigned long)&mouse_pointer_sprite,0x340,63);  

  switch(graze_shared_mem.state) {
  case FETCH_H65VIEW:
    // We loaded an H65 page, so display it, and allow interaction
    show_page();

    // Copy page URL back to normal strings for our use, e.g., for reload command
    lcopy(0xf800,(unsigned short)hostname,HOSTNAME_LEN);
    lcopy(0xf900,(unsigned short)path,PATH_LEN);
    port=graze_shared_mem.port;

    interact_page();
    break;

  case FETCH_H65FETCH_HTTPERROR:
    printf("\r\nHTTP error %d\r\n",graze_shared_mem.http_result);
    printf("Press almost any key to continue.\r\n");
    while(PEEK(0xD610)) POKE(0xD610,0); while(!PEEK(0xD610)) continue; POKE(0xD610,0);

    select_url();
    parse_url(0xD000 + 80);

    // Call GRAZEH65.M65 to fetch the page.
    fetch_page(hostname,port,path);
    break;    
        
  case FETCH_H65FETCH_DNSERROR:
    // Could not resolve hostname
    // XXX - should implement an error display
    
  case FETCH_H65FETCH_NOCONNECTION:
    // Could not connect to host
    // XXX - should implement an error display
    
    // Select A URL if we otherwise don't know what to do,
    // or we aborted fetching a page
  case FETCH_H65FETCH_ABORTED:
  case FETCH_SELECTURL:    
  default:
    select_url();
    parse_url(0xD000 + 80);

    // Call GRAZEH65.M65 to fetch the page.
    fetch_page(hostname,port,path);
    break;    
  }

}

void interact_page(void)
{
  unsigned char reload,i;

#if 0
  while(1) {
    POKE(0xD020,PEEK(0xD020)+1);
    if (PEEK(0xD610)) break;
  }
  while (PEEK(0xD610)) POKE(0xD610,0);
#endif

  while(1) {
    reload=0;
    update_mouse_position(1);

    // Check joystick / mouse-wheel
    last_j1=j1;
    j1 = PEEK(0xdc01);
    // Ideally we make sure we do only one per pulse
    // But as this main loop is not real fast, we don't have to worry about over doing it.
    //    if ((j1&8)&&(!(last_j1&8))) scroll_down(-8);
    //    if ((j1&4)&&(!(last_j1&4))) scroll_down(8);
    if (!(j1&8)) scroll_down(8);
    if (!(j1&4)) scroll_down(-8);
    
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
    case 0x13:
      // Home = Jump to top of page
      scroll_down(-9999);
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
    if (reload) {
      fetch_page(hostname,port,path);
    }
  }

}
