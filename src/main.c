
#if defined(__CPIK__)
#include <device/p18f97j60.h>
#include <interrupt.h>
#else
#include <p18f97j60.h>
#endif
#include "task.h"
#include "weeip.h"
#include "eth.h"

#pragma config FOSC=HSPLL, FOSC2=ON, WDT=OFF, WDTPS=1024, XINST=OFF, STVR=OFF, CP0=OFF
#pragma config MODE=MM, ETHLED=ON


/**
 * Inicializa a CPU, clock e interrupções.
 */
void cpu_init()
{
   //          76543210
   OSCCON =  0b00000000;
   OSCTUNE = 0b01000000;
   ADCON1 =  0b00001111;
   INTCON = 0;
   INTCON2 = 0;
   INTCON3 = 0;
   PIE1 = 0;
   PIE2 = 0;
   PIE3 = 0;
   RCONbits.IPEN = 0;
}

#if defined(__CPIK__)
__interrupt__ void hi_pri_ISR()
{
   if(INTCONbits.TMR0IF) {
      tick();
      INTCONbits.TMR0IF = 0;
   }

   if(PIR2bits.ETHIF && PIE2bits.ETHIE) {
      i_task_cancel(eth_task);
      i_task_add(eth_task, 0, 0);
      PIR2bits.ETHIF = 0;
      PIE2bits.ETHIE = 0;
   }
}
#else
#pragma interrupt interrupt_handler
void interrupt_handler(void)
{
   if(INTCONbits.TMR0IF) {
      tick(l);
      INTCON,bits.TMR0IF = 0;
   }
}

#pragma code InterruptVectorHigh = 0x08
void InterruptVectorHigh (void)
{
  _asm
    goto interrupt_handler
  _endasm
}
#pragma code

#pragma code InterruptVectorLow = 0x18
void InterruptVectorLow (void)
{
  _asm
    goto interrupt_handler
  _endasm
}
#pragma code
#endif

byte_t
pisca
   (byte_t p)
{
   LATJbits.LATJ0 = p;
   task_add(pisca, 32, !p);
}

SOCKET *s;
byte_t buf[20];

byte_t
comunica
   (byte_t p)
{
   socket_select(s);
   switch(p) {
      case WEEIP_EV_CONNECT:
         LATJbits.LATJ1 = 1;
         buf[0] = 'h';
         buf[1] = 'e';
         buf[2] = 'l';
         buf[3] = 'l';
         buf[4] = 'o';
         socket_send(buf, 5);
         break;
      case WEEIP_EV_DISCONNECT:
         LATJbits.LATJ1 = 0;
         socket_reset();
         socket_listen(55);
         break;
      case WEEIP_EV_DATA:
         buf[0] = 'o';
         buf[1] = 'k';
         socket_send(buf, 2);
         break;
   }
}

void main()
{
   cpu_init();

   task_init();


   mac_local.b[0] = 0x72;
   mac_local.b[1] = 0xb8;
   mac_local.b[2] = 0x79;
   mac_local.b[3] = 0xb1;
   mac_local.b[4] = 0x36;
   mac_local.b[5] = 0x38;

   ip_local.b[0] = 192;
   ip_local.b[1] = 168;
   ip_local.b[2] = 0;
   ip_local.b[3] = 12;

   ip_mask.b[0] = 255;
   ip_mask.b[1] = 255;
   ip_mask.b[2] = 255;
   ip_mask.b[3] = 0;

   weeip_init();

   TRISJbits.TRISJ1 = 0;
   TRISJbits.TRISJ0 = 0;
   LATJbits.LATJ0 = 0;
   LATJbits.LATJ1 = 0;

   TRISAbits.TRISA0 = 0;
   TRISAbits.TRISA1 = 0;


#if defined(__CPIK__)
   UNMASK_HI_PRI_IT;
#endif
   enable();

   s = socket_create(SOCKET_TCP);
   socket_set_callback(comunica);
   socket_set_rx_buffer(buf, 20);
   socket_listen(55);

   task_add(pisca, 10, 0);

   task_main();
}
