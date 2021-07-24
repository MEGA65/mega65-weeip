#include <stdio.h>

int main(int argc, char **argv)
{
  FILE *out=fopen(argv[2],"wb");
  FILE *in=fopen(argv[1],"r");

  // Write libpcap header
  unsigned char pcap_header[6*4]={
				 // Magic header
				 0xd4,0xc3,0xb2,0xa1,
				 // Major version number
				 0x02,0x00,
				 // Minor version number
				 0x04,0x00,
				 // GMT to local correction (normally always 0)
				 0x00,0x00,0x00,0x00,
				 // Accuracy of timestamps
				 0x00,0x00,0x00,0x00,
				 // Snap len (max packet size)
				 0x00,0x08,0x00,0x00,
				 // Network type
				 0x01,0x00,0x00,0x00};
  fwrite(pcap_header,6*4,1,out);
  
  char line[1024];
  line[0]=0; fgets(line,1024,in);
  while(line[0]) {
    line[0]=0; fgets(line,1024,in);
  }
}
