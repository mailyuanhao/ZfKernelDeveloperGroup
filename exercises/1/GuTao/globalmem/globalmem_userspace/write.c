#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdlib.h>

int main()
{
    srand(time(NULL));

    int i = 0;
    int j = 0;
    unsigned char one_data;
    int fd = open("/dev/globalmem", O_WRONLY);
    printf("write data:\n");
    if (fd > 0)
    {
        for(j=0; i<64; ++i,++j)
        {
            if(j>=8)
            {
                printf("\n");
                j=0;
            }
            
            one_data = rand();
            printf("0x%.2x ", one_data);
            write(fd, &one_data, 1);
        } 
        close(fd);
    }
    printf("\n");
    return 0;
}
