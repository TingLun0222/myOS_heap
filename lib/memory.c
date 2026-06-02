#include "types.h"

void *memset(void *ptr, int value, uint32_t num) {
    unsigned char *p = (unsigned char *) ptr;
    while (num-- > 0) {
        *p++ = (unsigned char) value;
    }
    return ptr;
}

char* memcpy(void* dest,const void* src, uint32_t num) 
{ 
   char* p_dest = (char*)dest; 
   const char* p_src = (const char*)src;

   while(num-- > 0)
     *p_dest++ = *p_src++;
   return dest; 
}

