
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#ifndef LLVM
#include "ascii.h"
#endif
#include "defs.h"

#include "weeip.h"
#include "checksum.h"
#include "eth.h"
#include "arp.h"

#include "mega65/memory.h"
#include "mega65/random.h"
#include "mega65/mouse.h"
#include "mega65/debug.h"

#include "shared_state.h"

#define GRAZE_MOUSE_POINTERS
#define GRAZE_PREPARE_NETWORK
#define GRAZE_C64_40COLUMNS
#include "../src/graze_common.c"

// Wait for key press before starting
//#define DEBUG_WAIT

unsigned char h65_error=0;
unsigned long block_addr,block_len;
unsigned short line_count;

void update_mouse_position(unsigned char do_scroll);

unsigned char text_row = 0;
unsigned short pixel_addr;
unsigned char char_code;

void h640_text_mode(void)
{
  // lower case
  POKE(0xD018, 0x16);

  // Normal text mode
  POKE(0xD054, 0x00);
  // H640, V400, fast CPU, extended attributes
  POKE(0xD031, 0xE8);
  // Adjust D016 smooth scrolling for VIC-III H640 offset
  POKE(0xD016, 0xC9);
  // 80 chars per line logical screen layout
  POKE(0xD058, 80);
  POKE(0xD059, 80 / 256);
  // Draw 80 chars per row
  POKE(0xD05E, 80);
  // Put 4KB screen at $C000
  POKE(0xD060, 0x00);
  POKE(0xD061, 0xc0);
  POKE(0xD062, 0x00);
  
  // Use the ASCII font we have loaded
  POKE(0xD069,0xf0);

  // 50 lines of text
  POKE(0xD07B, 50);

  lfill(0xc000, 0x20, 4000);
  // Clear colour RAM, while setting all chars to 4-bits per pixel
  lfill(0xff80000L, 0x0E, 4000);
}

void clear_text80(void)
{
  lfill(0xc000, 0x20,4000);
  lfill(0xff80000L, 0x01, 4000);
  text_row = 0;
}

void print_text80(unsigned char x, unsigned char y, unsigned char colour, char *msg)
{
  pixel_addr = 0xC000 + x + y * 80;
  while (*msg) {
    char_code = *msg;
#if USING_PETSCII_FONT    
    if (*msg >= 0xc0 && *msg <= 0xe0)
      char_code = *msg - 0x80;
    else if (*msg >= 0x40 && *msg <= 0x60)
      char_code = *msg - 0x40;
    else if (*msg >= 0x60 && *msg <= 0x7A)
      char_code = *msg - 0x20;
#endif
    POKE(pixel_addr + 0, char_code);
    lpoke(0xff80000L - 0xc000 + pixel_addr, colour);
    msg++;
    pixel_addr += 1;
  }
}

void println_text80(unsigned char colour, char *msg)
{
  if (text_row == 49) {
    lcopy(0xc000 + 80, 0xc000, 4000 - 80);
    lcopy(0xff80000 + 80, 0xff80000, 4000 - 80);
    lfill(0xc000 + 4000 - 80, 0x20, 80);
    lfill(0xff80000 + 4000 - 80, 0x01, 80);
  }
  print_text80(0, text_row, colour, msg);
  if (text_row < 49)
    text_row++;
}

int to_decimal(unsigned char *src, int bytes)
{
  int out = 0;
  int i;
  int x;
  for (i = 0; i < bytes; i++) {
    x = src[bytes - i - 1];
    out |= (x << (8 * i));
  }
  return out;
}

signed long screen_address_offset=0;
signed long screen_address_offset_max=0;

signed long position=0;
signed long max_position=0;


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

void fetch_page(char *hostname,int port,char *path)
{
  /*
    Call GRAZEH65 for H65 pages, or GRAZEGET for downloading files.

  */

  char *ext;

  // Show hourglass while loading new page
  POKE(0xD027,0x01); // make hourglass white
  lcopy((unsigned long)&mouse_hourglass_sprite,0x340,63);

  c64_40columns();
  
  // Setup URL port and strings at $F800 and $F900
  // for shared interaction with other modules
  lcopy((unsigned short)hostname,0xf800,256);
  lcopy((unsigned short)path,0xf900,256);
  graze_shared_mem.host_str_addr=0xf800;
  graze_shared_mem.path_str_addr=0xf900;
  graze_shared_mem.port=port;

  graze_shared_mem.job_id++;
  
  // Find . that separates extension
  ext=&path[strlen(path)-1];  
  while (ext>path&&ext[-1]!='.') ext--;  

  if ((ext[1]=='6'&&ext[2]=='5')&&(ext[0]==0x48||ext[0]==0x68)) {

    // Fetch H65 page
    mega65_dos_exechelper("GRAZEH65.M65");
    printf("ERROR: Could not load GRAZEH65.M65\n");
    while(1) POKE(0xd020,PEEK(0xd020)+1);
    
  } else {
    // Download file
    // XXX - We should check for .HTML, .PHP and other common extension for "normal" web pages,
    // and try to display them. But that will be done in the FETCH_GETFETCH_DOWNLOADED state
    // in main.  For now, we just need to call GRAZEGET.M65

    mega65_dos_exechelper("GRAZEGET.M65");
    printf("ERROR: Could not load GRAZEGET.M65\n");
    while(1) POKE(0xd020,PEEK(0xd020)+1);    
  }
  
}

unsigned char type_url[19]=
  {0x54,0x79,0x70,0x65,0x20, // Type
   0x6f,0x72,0x20, // or
   0x73,0x65,0x6c,0x65,0x63,0x74,0x20, // select
   0x55,0x52,0x4c, // URL
   0x3a // :
  };

char to_nybl(int v)
{
  v&=0xf;
  if (v<10) return '0'+v;
  return 'A'+v-10;
}

void to_hex(char *out,unsigned int v)
{
  out[0]=to_nybl(v>>12);
  out[1]=to_nybl(v>>8);
  out[2]=to_nybl(v>>4);
  out[3]=to_nybl(v>>0);
}

#ifdef LLVM
int
#else
void 
#endif
main(void)
{
  unsigned char i,reload;

  __asm__("sei");
  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  // Clear any queued key presses in $D610, so that
  // Graze doesn't try to use them as the start of a URL
  while(PEEK(0xD610)) POKE(0xD610,0);

  // Get initial mouse position
  mouse_update_position(NULL,NULL);
  mouse_warp_to(160,100);
  mouse_bind_to_sprite(0);
  mouse_update_pointer();
  mouse_set_bounding_box(24,50-20,320+23,250+20);

  // And export to shared state
  graze_shared_mem.mouse_x=160;
  graze_shared_mem.mouse_y=100;

  graze_shared_mem.job_id=0;
  
  // Enable sprite 0 as mouse pointer
  POKE(0xD015,0x01);
  // Sprite data from casette buffer
  POKE(0x7F8,0x340/0x40);
  lcopy((unsigned long)&mouse_hourglass_sprite,0x340,63);  

  // Loading files from SDcard corrupts first line of screen at $0400
  // Disable screen while we get ourselves organised...
  // (actually make it all black, so we can still show sprites)
  lfill(0xFF80000UL,0x00,1000); // all chars black
  POKE(0xD020UL,0x00); POKE(0xD021UL,0x00); // screen black

  // Load font
  i=read_file_from_sdcard("GRAZEFNT.M65",0xf000);
  
  // Then clear screen and set colour to green to allow showing of
  // network config process
  lfill(0xFF80000,0x0a,1000);
  lfill(0x0400,0x20,1000);
  POKE(0x286,0x0d);
  
  // Clear out URL history area
  lfill(0xD000L,0x20,4096);
  lcopy((unsigned long)type_url,0xD000L,19);

  // Give ethernet interface time to auto negotiate etc
#ifdef __llvm__
  printf("%c%ceNABLING eTHERNET...\n",0x93,0x0e);
#else
  printf("%c%cEnabling Ethernet...\n",0x93,0x0e);
#endif
  eth_init();
#ifdef __llvm__
  printf("cONFIGURING nETWORK...\n");
#else
  printf("Configuring network...\n");
#endif
  
  // Do DHCP config, and remember the configuration for helper programs
  graze_shared_mem.dhcp_configured=0;
  prepare_network();
  
  // XXX - Load URL history from disk image?

  lcopy((unsigned long)&mouse_pointer_sprite,0x340,63);  
  
  // Setup screen and show welcome message.
  POKE(0xD020,0);
  POKE(0xD021,0);
  POKE(0x0286,1);
  h640_text_mode();
  println_text80(0x81,"Graze - The Simple MEGA65 Browser");
  println_text80(1,"");
  println_text80(7,"Version 0.1");
  println_text80(1,"");
  println_text80(1,"Graze is like a web browser, but uses special H65 formatted pages instead of    ");
  println_text80(1,"HTML pages. This makes the browser very small and simple.                       ");
  println_text80(1,"But don't be deceived -- it supports text, hyper-links, graphics (including with");
  println_text80(1,"hyper-links behind them), as well as searching for and downloading files and    ");
  println_text80(1,"software for your MEGA65 home computer. And we didn't forget blinking text!");
  println_text80(1,"");
  println_text80(7,"Graze is designed to be used with a mouse. A MouSTer adaptor is a great way to  ");
  println_text80(7,"use a modern USB or wireless mouse with your MEGA65.");
  println_text80(1,"");
  println_text80(8,"Also, don't forget to connect the Ethernet port of your MEGA65 to a suitable    ");
  println_text80(8,"router or switch. Graze supports DHCP auto-configuration for IPv4 only.");
  println_text80(1,"");
  println_text80(1,"Enjoy!");
  println_text80(1,"");
  println_text80(1,"Please select or click on one of the options below:");
  println_text80(1,"");
  println_text80(0x0d,"G - Type in or choose from your browser history a URL to Goto an H65 web page");
  println_text80(1,"");
  println_text80(0x0d,"1 - Goto HTTP://10.42.0.1/index.h65");
  println_text80(1,"");
  println_text80(0x0d,"2 - Goto HTTP://192.168.178.20:8000/showdown65.h65");
  println_text80(1,"");
  println_text80(0x0d,"3 - Goto HTTP://www.badgerpunch.com/showdown65.h65");

  while(1) {
    unsigned short mx,my;
    unsigned char choice=0;
    update_mouse_position(0);

    mouse_update_position(&mx,&my);      

    if (my>0x70) {
      // Work out which choice row is currently selected
      // mouse res is in 320x200, so each 2 rows of text is 8 pixels
      choice=(my-0x78)>>3;
      for(i=20;i<27;i+=2) {
	if (i==(18+choice*2)) lfill(0xff80000+i*80,0x2d,80);
	else lfill(0xff80000+i*80,0x0d,80);
      }
    }
    
    // Check for mouse click on a choice
    // (Implement by cancelling a mouse-indicated choice, if the mouse isn't being clicked) 
    if (!mouse_clicked()) choice=0;

    // Scan keyboard for choices also
    if (PEEK(0xD610)) {
      switch(PEEK(0xD610)) {
      case 0x47: case 0x67: choice=1; break;
      case 0x31: choice=2; break;
      case 0x32: choice=3; break;
      case 0x33: choice=4; break;
      }
      POKE(0xD610,0);
    }

    // Act on user input
    switch(choice)
      {
      case 1:
	// Ask user to choose URL from history, or type one in
	graze_shared_mem.job_id++;
	graze_shared_mem.state=FETCH_SELECTURL;
	mega65_dos_exechelper("GRAZEERR.M65");
	break;
      case 2: fetch_page("10.42.0.1",80,"/index.h65"); break;
      case 3: fetch_page("192.168.178.20",8000,"/showdown65.h65"); break;
      case 4: fetch_page("www.badgerpunch.com",80,"/showdown65.h65"); break;
      }
  }
  
  //  fetch_page("files.mega65.org",80,"/INDEX.H65");
  fetch_page("192.168.178.20",8000,"/index.h65");
  //  fetch_page("zerobytesfree.io",80,"/index.h65");
  
}
