
extern IPV4 ip_broadcast;

void update_mouse_position(unsigned char do_scroll);

#ifdef GRAZE_MOUSE_POINTERS
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

#define SPRITE_PIXEL_ROW(A,B,C,D,E,F,G,H,I,J,K,L,M,N,O,P,Q,R,S,T,U,V,W,X) ((A?0x80:0)+(B?0x40:0)+(C?0x20:0)+(D?0x10:0)+(E?0x8:0)+(F?0x4:0)+(G?0x2:0)+(H?0x1:0)), ((I?0x80:0)+(J?0x40:0)+(K?0x20:0)+(L?0x10:0)+(M?0x8:0)+(N?0x4:0)+(O?0x2:0)+(P?0x1:0)), ((Q?0x80:0)+(R?0x40:0)+(S?0x20:0)+(T?0x10:0)+(U?0x8:0)+(V?0x4:0)+(W?0x2:0)+(X?0x1:0))

unsigned char mouse_hourglass_sprite[63]={
  SPRITE_PIXEL_ROW(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0),
  SPRITE_PIXEL_ROW(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0),
  SPRITE_PIXEL_ROW(0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0),
  SPRITE_PIXEL_ROW(0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,0,0,1,0,0,1,0,1,0,1,0,0,1,0,0,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,0,0,0,1,0,0,1,0,1,0,0,1,0,0,0,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,0,0,0,0,1,0,0,1,0,0,1,0,0,0,0,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,0,0,0,1,0,0,0,1,0,0,0,1,0,0,0,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,0,0,1,0,0,0,0,0,0,0,0,0,1,0,0,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,1,0,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,0,1,0,0,0,0,0,1,0,1,0,0,0,0,0,1,0,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,0,1,0,0,0,0,0,1,0,1,0,1,0,0,0,0,0,1,0,0,0,0),
  SPRITE_PIXEL_ROW(0,0,1,0,0,0,0,0,1,0,1,0,1,0,1,0,0,0,0,0,1,0,0,0),
  SPRITE_PIXEL_ROW(0,0,1,0,0,0,0,1,0,1,0,1,0,1,0,1,0,0,0,0,1,0,0,0),
  SPRITE_PIXEL_ROW(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0),
  SPRITE_PIXEL_ROW(1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,1,0),

};

#endif

#ifdef GRAZE_C64_40COLUMNS
void c64_40columns(void)
{
  // Reset video mode to C64 40 column mode while loading
  POKE(0xD054,0);
  POKE(0xD031,0);
  POKE(0xD011,0x1B);
  POKE(0xD016,0xC8);
  POKE(0xD018,0x16);
}
#endif

#ifdef GRAZE_PREPARE_NETWORK

#include "dhcp.h"

void prepare_network(void)
{
  unsigned char i;

  // XXX - Complain if MAC address is invalid, and direct use to configure it using configure?
  lcopy(0xFFD36E9,(unsigned long)&mac_local.b[0],6);
#ifdef __llvm__
  printf("mac ADDRESS IS %02x",mac_local.b[0]);
#else
  printf("MAC address is %02x",mac_local.b[0]);
#endif
  for(i=1;i<6;i++) printf(":%02x",mac_local.b[i]);
  printf("\n");
  
  // Setup WeeIP
  weeip_init();
  task_cancel(eth_task);
  task_add(eth_task, 0, 0,"eth");

  // Do DHCP auto-configuration
  dhcp_configured=graze_shared_mem.dhcp_configured;
  if (!dhcp_configured) {
    //    printf("\nRequesting IP...\n");
    dhcp_autoconfig();
    while(!dhcp_configured) {
      task_periodic();
      // Let the mouse move around
      update_mouse_position(0);
    }
    // Store DHCP lease information for later recall
    graze_shared_mem.dhcp_configured=1;
    graze_shared_mem.dhcp_myip=ip_local;
    graze_shared_mem.dhcp_dnsip=ip_dnsserver;
    graze_shared_mem.dhcp_netmask=ip_mask;
    graze_shared_mem.dhcp_gatewayip=ip_gate;
  } else {
    // Restore DHCP lease configuration
    ip_local=graze_shared_mem.dhcp_myip;
    ip_dnsserver=graze_shared_mem.dhcp_dnsip;
    ip_mask=graze_shared_mem.dhcp_netmask;
    ip_gate=graze_shared_mem.dhcp_gatewayip;
    // Re-constitute ip_broadcast from IP address and mask
    for(i=0;i<4;i++) ip_broadcast.b[i]=(0xff&(0xff^ip_mask.b[i]))|ip_local.b[i];
#ifdef __llvm__
    printf("nETWORK ALREADY CONFIGURED.\n");	   
#else
    printf("Network already configured.\n");
#endif
  }
}      
#endif
