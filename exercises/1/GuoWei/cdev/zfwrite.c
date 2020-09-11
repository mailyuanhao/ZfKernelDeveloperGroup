#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<stdbool.h>

typedef union
{
	unsigned int rd_i;
	unsigned char rd_c[4];
}RAND_NU;

int MAX_LOOP = 10;

int main(){
   int ret, fd, ct;
   RAND_NU randx;
   printf("Starting device write code example...\n");
   fd = open("/dev/zfchar", O_RDWR | O_APPEND);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }

   while (true)
   {
      randx.rd_i = rand();
      printf("write number is 0x%02x%02x%02x%02x. \n", randx.rd_c[0], randx.rd_c[1], randx.rd_c[2], randx.rd_c[3]);
      ret = write(fd, &randx, sizeof(randx)); // Send the string to the LKM
      if (ret < 0){
         perror("Failed to write the message to the device.");
         return errno;
      }
      sleep(4);
   }

   return 0;
}
