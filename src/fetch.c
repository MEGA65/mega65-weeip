
#include <stdio.h>
#include <string.h>

#include "ascii.h"
#include "defs.h"

#include "memory.h"
#include "random.h"
#include "mouse.h"
#include "debug.h"

#include "shared_state.h"

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

unsigned char h65_error=0;
unsigned long block_addr,block_len;
unsigned short line_count;

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
    Call FETCHH65 for H65 pages, or FETCHGET for downloading files.

  */

  char *ext;

  // Setup URL port and strings at $F800 and $F900
  // for shared interaction with other modules
  lcopy((unsigned short)hostname,0xf800,256);
  lcopy((unsigned short)path,0xf900,256);
  fetch_shared_mem.host_str_addr=0xf800;
  fetch_shared_mem.path_str_addr=0xf900;
  fetch_shared_mem.port=port;

  fetch_shared_mem.job_id++;
  
  // Find . that separates extension
  ext=&path[strlen(path)-1];  
  while (ext>path&&ext[-1]!='.') ext--;  

  if ((ext[1]=='6'&&ext[2]=='5')&&(ext[0]==0x48||ext[0]==0x68)) {

    // Fetch H65 page
    mega65_dos_exechelper("FETCHH65.M65");
    printf("ERROR: Could not load FETCHH65.M65\n");
    while(1) POKE(0xd020,PEEK(0xd020)+1);
    
  } else {
    // Download file
    // XXX - We should check for .HTML, .PHP and other common extension for "normal" web pages,
    // and try to display them. But that will be done in the FETCH_GETFETCH_DOWNLOADED state
    // in main.  For now, we just need to call FETCHGET.M65

    mega65_dos_exechelper("FETCHGET.M65");
    printf("ERROR: Could not load FETCHGET.M65\n");
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

void main(void)
{
  unsigned char i,reload;

  __asm__("sei");
  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));

  // Clear any queued key presses in $D610, so that
  // Fetch doesn't try to use them as the start of a URL
  while(PEEK(0xD610)) POKE(0xD610,0);
  
  i=read_file_from_sdcard("FETCHFNT.M65",0xf000);

  // Get initial mouse position
  mouse_update_position(NULL,NULL);
  mouse_warp_to(160,100);
  mouse_bind_to_sprite(0);
  mouse_update_pointer();
  mouse_set_bounding_box(24,50-20,320+23,250+20);

  // And export to shared state
  fetch_shared_mem.mouse_x=160;
  fetch_shared_mem.mouse_y=100;

  fetch_shared_mem.job_id=0;
  
  // Enable sprite 0 as mouse pointer
  POKE(0xD015,0x01);
  // Sprite data from casette buffer
  POKE(0x7F8,0x340/0x40);
  lcopy((unsigned long)&mouse_pointer_sprite,0x340,63);  

  // Clear out URL history area
  lfill(0xD000L,0x20,4096);
  lcopy((unsigned long)type_url,0xD000L,19);

  // XXX - Load URL history from disk image?

  //  fetch_page("files.mega65.org",80,"/INDEX.H65");
  //  fetch_page("192.168.178.20",80,"/index.h65");
  fetch_page("zerobytesfree.io",80,"/index.h65");
  
}
