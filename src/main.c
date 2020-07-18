
#include "task.h"
#include "weeip.h"
#include "eth.h"

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

SOCKET *s;
byte_t buf[20];

/* Function that is used as a call-back on socket events
 * */
byte_t comunica (byte_t p)
{
   socket_select(s);
   switch(p) {
      case WEEIP_EV_CONNECT:
         buf[0] = 'h';
         buf[1] = 'e';
         buf[2] = 'l';
         buf[3] = 'l';
         buf[4] = 'o';
         socket_send(buf, 5);
         break;
      case WEEIP_EV_DISCONNECT:
         socket_reset();
         socket_listen(55);
         break;
      case WEEIP_EV_DATA:
         buf[0] = 'o';
         buf[1] = 'k';
         socket_send(buf, 2);
         break;
   }

   return 0;
}

void main(void)
{
  
  srand(random32(0));
  POKE(0,65);
  mega65_io_enable();
  
   // Set MAC address
   mac_local.b[0] = 0x72;
   mac_local.b[1] = 0xb8;
   mac_local.b[2] = 0x79;
   mac_local.b[3] = 0xb1;
   mac_local.b[4] = 0x36;
   mac_local.b[5] = 0x38;

   // Set our IP
   ip_local.b[0] = 192;
   ip_local.b[1] = 168;
   ip_local.b[2] = 0;
   ip_local.b[3] = 12;

   // Set Netmask
   ip_mask.b[0] = 255;
   ip_mask.b[1] = 255;
   ip_mask.b[2] = 255;
   ip_mask.b[3] = 0;

   weeip_init();


   s = socket_create(SOCKET_TCP);
   socket_set_callback(comunica);
   socket_set_rx_buffer(buf, 20);
   socket_listen(55);

   task_add(pisca, 10, 0);

   while(1) {
     // XXX Actually only call it periodically
     task_periodic();
   }
}
