char cdecl read_file_from_sdcard(char *filename,unsigned long load_address);
char cdecl mega65_dos_exechelper(char* filename);

struct __fetch_shared_mem {
  #define FETCH_SELECTURL 0
  #define FETCH_H65VIEW 1
  #define FETCH_H65FETCH_DNSERROR 2
  #define FETCH_H65FETCH_NOCONNECTION 3
  #define FETCH_H65FETCH_ABORTED 4
  #define FETCH_H65FETCH_HTTPERROR 5
  unsigned char state;

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
};

#define fetch_shared_mem     (*(struct __fetch_shared_mem*)0x3c0)
