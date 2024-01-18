#ifdef LLVM
#define cdecl
#endif

char cdecl read_file_from_sdcard(char *filename,unsigned long load_address);
char cdecl mega65_dos_exechelper(char* filename);

struct __graze_shared_mem {
  #define FETCH_SELECTURL 0
  #define FETCH_H65VIEW 1
  #define FETCH_H65FETCH_DNSERROR 2
  #define FETCH_H65FETCH_NOCONNECTION 3
  #define FETCH_H65FETCH_ABORTED 4
  #define FETCH_H65FETCH_HTTPERROR 5
  #define FETCH_PAGEFETCHERROR 6
  unsigned char state;

  // H65 response code
  // (see h65.h for definitions)
  unsigned char h65_error;
  
  // HTTP response code
  unsigned short http_result;
  
  // Counter so that we know how many times things are being called
  unsigned short job_id;
  
  // Mouse position
  unsigned short mouse_x, mouse_y;
  
  // URLs
  unsigned short port;
  unsigned short host_str_addr;
  unsigned short path_str_addr;

  // video mode info
  unsigned char d054_bits,d031_bits,line_width,line_display_width,border_colour,screen_colour,text_colour,char_page,d016_bits;
  unsigned short line_count;

  // DHCP lease info
  unsigned char dhcp_configured;
  IPV4 dhcp_myip;
  IPV4 dhcp_gatewayip;
  IPV4 dhcp_dnsip;
  IPV4 dhcp_netmask;
  
};

#define  graze_shared_mem     (*(volatile struct __graze_shared_mem*)0x3c0)
