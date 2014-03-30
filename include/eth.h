
#ifndef __ETHH__
#define __ETHH__
#include "defs.h"
#include "inet.h"
extern IPV4 ip_mask;
extern IPV4 ip_gate;
extern EUI48 mac_local;
void eth_read(buffer_t dest, uint16_t tam);
void eth_write(buffer_t orig, uint16_t tam);
void eth_set(byte_t v, uint16_t tam);
bool_t eth_clear_to_send();
void eth_drop();
byte_t eth_task(byte_t sig);
bool_t eth_ip_send();
void eth_arp_send(EUI48 *mac);
void eth_packet_send(uint16_t size);
void eth_init();
void eth_disable();
void eth_enable();
#endif
