/*
  Simple loader program for fetch.

  It should:

  1. Display a splash screen?
  2. Load ASCII font at $F000
  3. Load browsing history / book marks into memory
  4. Work out if we can use attic RAM for big TCP RX buffer,
     and load appropriate version of fetch?
  5. Load and run fetch

  For now, only 2 and 5 are vital.

*/

char cdecl read_file_from_sdcard(char *filename,unsigned long load_address);
char cdecl mega65_dos_exechelper(char* filename);

#include <stdio.h>
#include "memory.h"
#include "ascii.h"

int main(void)
{
  __asm__("sei");
  mega65_io_enable();
  POKE(0,65);
  
  read_file_from_sdcard("FETCHFNT.M65",0xf000);
  mega65_dos_exechelper("FETCHM.M65");
  printf("ERROR: Could not load FETCHM.M65\n");
  return 0;
}
