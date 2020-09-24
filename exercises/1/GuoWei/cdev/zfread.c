#include<stdio.h>
#include<stdlib.h>
#include<errno.h>
#include<fcntl.h>
#include<string.h>
#include<unistd.h>
#include<stdbool.h>

#define BUFFER_LENGTH 256               ///< The buffer length (crude but fine)
static unsigned char receive[BUFFER_LENGTH];     ///< The receive buffer from the LKM

int main(){
   int ret, fd;
   printf("Starting device test code example...\n");
   fd = open("/dev/zfchar", O_RDONLY);             // Open the device with read/write access
   if (fd < 0){
      perror("Failed to open the device...");
      return errno;
   }

   int ct = 0;
   int i = 0;

   while (true)
   {
      printf("Reading from the device is ");
      ret = read(fd, receive + i, 1);        // Read the response from the LKM
      if (ret < 0){
         perror("Failed to read the message from the device.");
         return errno;
      }
      sleep(1);

      printf("0x%02x\n", receive[i]);
	   i++;
   }
  
   printf("\nEnd of the program\n");
   return 0;
}
