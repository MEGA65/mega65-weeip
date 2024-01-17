
#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "task.h"
#include "weeip.h"
#include "eth.h"
#include "arp.h"
#include "dns.h"
#include "dhcp.h"

#include "mega65/tests.h"
#include "mega65/memory.h"
#include "mega65/random.h"
#include "mega65/mouse.h"
#include "mega65/debug.h"

#include "h65.h"

#include "shared_state.h"

// Wait for key press before starting
//#define DEBUG_WAIT

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
  }
  printf("My IP is %d.%d.%d.%d\n",
	 ip_local.b[0],ip_local.b[1],ip_local.b[2],ip_local.b[3]);
}      

#ifdef LLVM
int
#else
void 
#endif
main(void)
{
  unsigned char i,reload;

  // Clear quote mode before printing
  POKE(0xf,0);
  
  // Enable logging of ethernet activity on the serial monitor interface
  eth_log_mode=ETH_LOG_TX; // ETH_LOG_RX|ETH_LOG_TX;

  POKE(0,65);
  mega65_io_enable();
  srand(random32(0));
  
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

  while(!dhcp_configured) {
    POKE(0xD020,PEEK(0xD020)+1);
  }

  // Indicate that DHCP succeeded
  unit_test_report(1, 0, TEST_PASS);
  unit_test_report(1, 1, TEST_DONEALL);

  printf("PASS Ethernet\n"
	 "\n"
	 "1. Tests.\n");
  if (PEEK(0xD69D)&0x04) {
    printf(
	   "2. Flash QSPI\n"
	   );
  } else {
    printf("\nNOTE: SW3 dip switch 3 must be on for flashing.\n");
  }
  __asm__ ("sei");
  
  while(PEEK(0xD610)) POKE(0xD610,0);
  while(!PEEK(0xD610)) POKE(0xD020,PEEK(0xD020)+1);
  if (PEEK(0xD610)==0x32) mega65_dos_exechelper("spiflash.m65");
  else mega65_dos_exechelper("prodtest.m65");

  // ERROR: Should never get here
  while(1) POKE(0xD021,PEEK(0xD021)+1);
  
}
