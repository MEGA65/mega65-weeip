#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdlib.h>

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

  unsigned char bytes[2048];
  int byte_count=0;  
  char line[1024];
  char rxtx[1024];
  int hour,min,sec,raster;
  char last_rxtx[1024];
  int last_hour,last_min,last_sec,last_raster;
  int valid=0;
  line[0]=0; fgets(line,1024,in);
  while(line[0]) {

    if (sscanf(line,"%d:%d:%d/%d ETH %s\n",
	       &hour,&min,&sec,&raster,rxtx)==5) {

      if (valid) {
	if (!strncmp(last_rxtx,"RX",2)) byte_count=bytes[0]+((bytes[1]&0x7)<<8);
	fprintf(stderr,"%02d:%02d:%02d/%d ETH %s : %d bytes\n",
		last_hour,last_min,last_sec,last_raster,last_rxtx,byte_count);

	unsigned char packet_header[16];
	bzero(packet_header,16);
	int ts=last_hour*3600+last_min*60+last_sec;
	packet_header[0]=ts>>0;
	packet_header[1]=ts>>8;
	packet_header[2]=ts>>16;
	packet_header[3]=ts>>24;
	// Assume all rasters are on first frame of second,
	// which we cannot actually confirm.
	// But it will at least give some timing info between
	// frames.
	packet_header[4]=(last_raster*10000000/50/323)>>0;
	packet_header[5]=(last_raster*10000000/50/323)>>8;
	packet_header[8]=byte_count;
	packet_header[9]=byte_count>>8;
	packet_header[12]=byte_count;
	packet_header[13]=byte_count>>8;
	fwrite(packet_header,16,1,out);
	if (!strncmp(last_rxtx,"RX",2))
	  fwrite(&bytes[2],byte_count,1,out);
	else 
	  fwrite(bytes,byte_count,1,out);
	
	byte_count=0;
	bzero(bytes,2048);
      }
      
      valid=1;

      last_hour=hour;
      last_min=min;
      last_sec=sec;
      last_raster=raster;
      strcpy(last_rxtx,rxtx);          
    }

    int d[16],addr;
    if (sscanf(line,"  %x : %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x %x",
	       &addr,
	       &d[0],&d[1],&d[2],&d[3],
	       &d[4],&d[5],&d[6],&d[7],
	       &d[8],&d[9],&d[10],&d[11],
	       &d[12],&d[13],&d[14],&d[15])==17) {
      if (addr>=0&&addr<2048) {
	for(int i=0;i<16;i++) {
	  bytes[addr+i]=d[i];
	}
	byte_count=addr+16;
      }
    }
    
    line[0]=0; fgets(line,1024,in);
  }
}
