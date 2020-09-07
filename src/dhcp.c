#include <stdio.h>

#include "weeip.h"
#include "eth.h"
#include "arp.h"
#include "dns.h"

bool_t dhcp_autoconfig(void)
{
   // Set our IP
   ip_local.b[0] = 203;
   ip_local.b[1] = 19;
   ip_local.b[2] = 107;
   ip_local.b[3] = 64;

   // Set gateway IP
   ip_gate.b[0] = 203;
   ip_gate.b[1] = 19;
   ip_gate.b[2] = 107;
   ip_gate.b[3] = 1;
      
   // Set Netmask
   ip_mask.b[0] = 255;
   ip_mask.b[1] = 255;
   ip_mask.b[2] = 255;
   ip_mask.b[3] = 0;

   // Set DNS server
   ip_dnsserver.b[0] = 8;
   ip_dnsserver.b[1] = 8;
   ip_dnsserver.b[2] = 8;
   ip_dnsserver.b[3] = 8;

   return 1;
}
