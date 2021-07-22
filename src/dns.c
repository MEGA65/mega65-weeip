#include <stdio.h>
#include <string.h>

#include "weeip.h"
#include "eth.h"
#include "arp.h"
#include "dns.h"

#include "memory.h"
#include "random.h"

unsigned char dns_query_returned=0;
IPV4 dns_return_ip;
SOCKET *dns_socket;
unsigned char dns_query[1024];
uint16_t dns_query_len=0;
unsigned char dns_buf[1024];

void dns_construct_hostname_to_ip_query(char *hostname)
{  
  unsigned char prefix_position,i;
  unsigned char field_len;
  
  // Now form our DNS query
  // 16-bit random request ID:  
  dns_query[0]=random32(256);
  dns_query[1]=random32(256);
  // Request flags: Is request, please recurse etc
  dns_query[2]=0x01;
  dns_query[3]=0x00;
  // QDCOUNT = 1 (one question follows)
  dns_query[4]=0x00;
  dns_query[5]=0x01;
  // ANCOUNT = 0 (no answers follow)
  dns_query[6]=0x00;
  dns_query[7]=0x00;
  // NSCOUNT = 0 (no records follow)
  dns_query[8]=0x00;
  dns_query[9]=0x00;
  // ARCOUNT = 0 (no additional records follow)
  dns_query[10]=0x00;
  dns_query[11]=0x00;
  dns_query_len=12;
  
  // Now convert dotted hostname to DNS field format.
  // This involves changing each . to the length of the following field,
  // adding a $00 to the end, and prefixing the whole thing with the
  // length of the first part.
  // This is most easily done by reserving the prefix byte first, and whenever
  // we hit a . or end of string, updating the previous prefix byte.
  prefix_position=dns_query_len++;   
  lcopy((unsigned long)hostname,(unsigned long)&dns_query[dns_query_len],strlen(hostname)+1);
  field_len=0;
  for(i=0;;i++) {
    if (hostname[i]=='.'||hostname[i]==0) {
      dns_query[prefix_position]=field_len;
      prefix_position=dns_query_len+i;
      field_len=0;
    } else field_len++;
    if (hostname[i]==0) break;
  }
  dns_query_len+=strlen(hostname)+1;
  
  // QTYPE = 0x0001 = "A records"
  dns_query[dns_query_len++]=0x00;
  dns_query[dns_query_len++]=0x01;
  // QCLASS = 0x0001 = "internet addressses
  dns_query[dns_query_len++]=0x00;
  dns_query[dns_query_len++]=0x01;
}  

byte_t dns_reply_handler (byte_t p)
{
  unsigned int ofs;
  unsigned char i,j;

  //  printf("DNS packet seen.\n");
  
  socket_select(dns_socket);
  switch(p) {
  case WEEIP_EV_DATA:

    // Check that query ID matches
    if (dns_buf[0]!=dns_query[0]) break;
    if (dns_buf[1]!=dns_query[1]) break;

    // Check that it is a reply
    if ((dns_buf[2]!=0x81)&&(dns_buf[2]!=0x85)) break;

    // Check if we have at least one answer
    if (!(dns_buf[6]||dns_buf[7])) {
      //      printf("DNS response contained no answers.\n");
      break;
    }

    // Skip over the question text (HACK: Search for first $00 byte)
    ofs=0xc; while(dns_buf[ofs]) ofs++;
    // Then skip that $00 byte
    ofs++;

    // Skip over query type and class if correct
    if ((dns_buf[ofs]==0x00)&&(dns_buf[ofs+1]==0x01)) {
	ofs+=2;
	if ((dns_buf[ofs]==0x00)&&(dns_buf[ofs+1]==0x01)) {
	  ofs+=2;
	  // Now we are at the start of the answer section
	  // Assume that answers will be pointers to the name in the query.
	  // For this we look for fixed value $c0 $0c indicating pointer to the name in the query
	  if ((dns_buf[ofs]==0xc0)&&(dns_buf[ofs+1]==0x0c)) {
	    ofs+=2;
	    // Check if it is a CNAME, in which case we need to re-issue the request for the
	    // CNAME
	    if ((dns_buf[ofs]==0x00)&&(dns_buf[ofs+1]==0x05)) {
	      
	      ofs+=2;
	      // printf("DNS server responded with a CNAME\n");
	      // Skip TTL and size
	      ofs+=8;
	      // We should now have len+string tuples following.
	      // So decode those into a hostname string, and resolve that instead
	      // We decode them into place in dns_buf to avoid having a separate buffer
	      i=0;
	      while(dns_buf[ofs]) {

		// Expand compressed name pointers
		if (dns_buf[ofs]>=0xc0) {
		  ofs=dns_buf[ofs+1];
		}
		
		j=0;
		if (i) dns_buf[i++]='.';
		while(dns_buf[ofs]--) {
		  dns_buf[i++]=dns_buf[ofs+1+j]; j++;
		}
		ofs+=j+1;
		dns_buf[i]=0;
	      }
	      
	      printf("Resolving CNAME '%s' ...\n",dns_buf);
	      dns_construct_hostname_to_ip_query(dns_buf);
	      socket_send(dns_query,dns_query_len);
	      
	    }
	    // Then we check that answer type is $00 $01 = "type a"
	    if ((dns_buf[ofs]==0x00)&&(dns_buf[ofs+1]==0x01)) {
	      ofs+=2;
	      // Then we check that answer class is $00 $01 = "IPv4 address"
	      if ((dns_buf[ofs]==0x00)&&(dns_buf[ofs+1]==0x01)) {
		ofs+=2;
		// Now we can just skip over the TTL and size, by assuming its a 4 byte
		ofs+=6;
		// IP address
		dns_return_ip.b[0]=dns_buf[ofs+0];
		dns_return_ip.b[1]=dns_buf[ofs+1];
		dns_return_ip.b[2]=dns_buf[ofs+2];
		dns_return_ip.b[3]=dns_buf[ofs+3];
		dns_query_returned=1;
		break;
	      }
	    }
	  }
	}
    }
    
    break;
  }
  return 0;
}

unsigned char offset=0;
unsigned char bytes=0;
unsigned char value=0;

bool_t dns_hostname_to_ip(char *hostname,IPV4 *ip)
{
  EUI48 mac;
  unsigned char next_retry,retries;

  // Check if IP address, and if so, parse directly.
  offset=0; bytes=0; value=0;
  while(hostname[offset]) {
    if (hostname[offset]=='.') {
      ip->b[bytes++]=value;
      value=0;
      if (bytes>3) break;
    } else if (hostname[offset]>='0'&&hostname[offset]<='9') {
      value=value*10; value+=hostname[offset]-'0';
    } else
      // Not a digit or a period, so its not an IP address
      break;
    offset++;
  }
  if (bytes==3&&(!hostname[offset])) {ip->b[3]=value; return 1; }
  
  dns_socket = socket_create(SOCKET_UDP);
  socket_set_callback(dns_reply_handler);
  socket_set_rx_buffer(dns_buf,1024);
  
  // Before we get any further, send an ARP query for the DNS server
  // (or if it isn't on the same network segment, for our gateway.)
  arp_query(&ip_dnsserver);
  arp_query(&ip_gate);
  // Then wait until we get a reply.
  while((!query_cache(&ip_dnsserver,&mac)) &&(!query_cache(&ip_gate,&mac)) ) {
    task_periodic();     
  }   
  
  socket_select(dns_socket);
  socket_connect(&ip_dnsserver,53);

  dns_construct_hostname_to_ip_query(hostname);

  socket_send(dns_query,dns_query_len);

  // Run normal network state machine
  // XXX Call-back handlers for other network tasks can still occur
  dns_query_returned=0;

  // Retry for approx 30 seconds (will be slightly longer on NTSC, as we
  // time retries based on elapsed video frames).
  retries=30;
  next_retry=PEEK(0xD7FA)+50;
  
  while(!dns_query_returned) {
    task_periodic();

    // Detect timeout, and retry for ~30 seconds
    if (PEEK(0xD7FA)==next_retry) {
      if (!retries) return 0;
      socket_select(dns_socket);
      socket_send(dns_query,dns_query_len);
      retries--;
      next_retry=PEEK(0xD7FA)+50;
    }

  }

  // Copy resolved IP address
  ip->b[0]=dns_return_ip.b[0];
  ip->b[1]=dns_return_ip.b[1];
  ip->b[2]=dns_return_ip.b[2];
  ip->b[3]=dns_return_ip.b[3];

  socket_release(dns_socket);

  return 1;
}
