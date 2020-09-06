#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

int main()
{
    int i = 0;
    int j = 0;
    char buffer[64];
    int fd = open("/dev/globalmem", O_RDONLY);
    printf("read data:\n");
    if (fd > 0)
    {
        read(fd, buffer, sizeof(buffer));
        for(j=0; i<sizeof(buffer); ++i,++j)
        {
            if(j>=8)
            {
                printf("\n");
                j=0;
            }
            
            printf("0x%.2x ", buffer[i]);
        } 
        close(fd);
    }
    printf("\n");
    return 0;
}
