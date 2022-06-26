char cdecl read_file_from_sdcard(char *filename,unsigned long load_address);
char cdecl mega65_dos_exechelper(char* filename);

struct __fetch_shared_mem {
  #define FETCH_SUCCESS 0
  #define FETCH_DNSERROR 1
  #define FETCH_NOCONNECTION 2
  #define FETCH_ABORTED 3
  unsigned char result;

  // Mouse position
  unsigned short mouse_x, mouse_y;
  
  // URLs
  unsigned short port;
  unsigned short host_str_addr;
  unsigned short path_str_addr;
};

#define fetch_shared_mem     (*(struct __fetch_shared_mem*)0x380)
