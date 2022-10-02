#include <stdio.h>
//#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>

typedef struct pcap_hdr_s {
  uint32_t magic_number;   /* magic number */
  uint16_t version_major;  /* major version number */
  uint16_t version_minor;  /* minor version number */
  int32_t  thiszone;       /* GMT to local correction */
  uint32_t sigfigs;        /* accuracy of timestamps */
  uint32_t snaplen;        /* max length of captured packets, in octets */
  uint32_t network;        /* data link type */
} pcap_hdr_t;

typedef struct pcaprec_hdr_s {
  uint32_t ts_sec;         /* timestamp seconds */
  uint32_t ts_usec;        /* timestamp microseconds */
  uint32_t incl_len;       /* number of octets of packet saved in file */
  uint32_t orig_len;       /* actual length of packet */
} pcaprec_hdr_t;

unsigned char dat[8192*1024];
int dat_len=0;  

int main(int argc,char **argv)
{
  if (argc!=3) {
    fprintf(stderr,"usage: hex2pcap <input.raw> <out.pcap>\n");
    exit(-1);
  }

  FILE *inf=fopen(argv[1],"rb");
  if (!inf) {
    fprintf(stderr,"ERROR: Could not open raw file '%s'\n",argv[1]);
    exit(-1);
  }
  dat_len=fread(dat,1,sizeof(dat),inf);
  fclose(inf);
  fprintf(stderr,"NOTE: Processing %d bytes\n",dat_len);	 

  FILE *f=fopen(argv[2],"wb");
  if (!f) {
    fprintf(stderr,"ERROR: Could not write to file '%s'\n",argv[2]);
    exit(-1);
  }

  pcap_hdr_t hdr;  
  hdr.magic_number=0xa1b2c3d4;
  hdr.version_major=2;
  hdr.version_minor=4;
  hdr.thiszone=0;
  hdr.sigfigs=0;
  hdr.snaplen=2048;
  hdr.network=1; // ethernet
  fwrite(&hdr,sizeof(hdr),1,f);

  char line[1024];
  unsigned char pkt[2048];
  int txP=0;
  int hour,min,sec,frames;
  char txrx[1024];
  int addr,b[16];

  for(int ofs=0;ofs<dat_len;ofs+=2048) {
    int plen=dat[ofs]+(dat[ofs+1]<<8);
    plen&=0x7ff;
    fprintf(stderr,"DEBUG: Packet of 0x%04x bytes\n",plen);

    if (plen) {
      pcaprec_hdr_t h;
      h.ts_sec=0;
      h.ts_usec=frames;
      h.incl_len=plen;
      h.orig_len=plen;
      fwrite(&h,sizeof(h),1,f);
      fwrite(&dat[ofs+2],plen,1,f);
      fprintf(stderr,"DEBUG: Bytes = %02x %02x %02x ...\n",
	      dat[ofs+2],dat[ofs+3],dat[ofs+4]);
    }
  }

  fclose(f);
  
}
