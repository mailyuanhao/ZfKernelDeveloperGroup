#include <stdio.h>
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <unistd.h>
#include <uuid/uuid.h>
#include <string>
#include <sys/ioctl.h>
#include <sys/select.h>
#include <poll.h>

#define MAX_LENGTH 4096
#define LXC_IOCTL_GET_FIFO_LEN 0x80044c01 

//sudo apt-get install uuid-dev
std::string create_uuid()
{
	uuid_t uuid;
    char str[36] = { 0 };
    uuid_generate(uuid);
	uuid_unparse(uuid, str);

	return std::string(str);			  
}

int run_writer(void)
{
	int fd = open("/dev/lxcdev0", O_RDWR);
	if (fd == -1)
	{
		perror("open error");
		return 0;
	}
	else
	{
		printf("open success\n");
	}
	
	printf("write some times\n");
	for (int i = 0; i < 100; ++ i)
	{
		sleep(1);
		std::string uuid = create_uuid();
		printf("write data:%s\n", uuid.c_str());

		ssize_t ret = write(fd, uuid.c_str(), uuid.length());
		if (-1 == ret)
		{
			perror("write data");
		}
		else
		{
			printf("write length %d success\n", uuid.length());
		}
	}

	close(fd);
	printf("writer exit now\n");
	return 0;
}

int run_reader(void)
{
	int fd = open("/dev/lxcdev0", O_RDWR);
	if (-1 == fd)
	{
		perror("open error");
		return 0;
	}
	else
	{
		printf("open success\n");
	}

	char buff[MAX_LENGTH + 1] = { 0 };

	while (1)
	{
		sleep(1);
		memset (buff, 0, sizeof(buff));
		ssize_t ret = read(fd, buff, MAX_LENGTH-1);
		if (-1 == ret)
		{
			perror("read data");
		}
		else if (ret > 0)
		{
			char * cc = NULL;
			for (size_t i = 0; i < ret; ++ i)
			{
				cc = buff + i;	
				*cc ^= 0x55;
			}

			printf("read data:%s\n", buff);
		}
		else if (ret == 0)
		{
			printf("there is no data now\n");
		}
	}

	close(fd);
	printf("reader exit now\n");
	return 0;
}

int run_test(char *cmd)
{
	if (strcmp(cmd, "-guid") == 0)
	{
		std::string uuid = create_uuid();
		printf("uuid:%s\n", uuid.c_str());
	}
	else if (strcmp(cmd, "-io") == 0)
	{
		
		int fd = open("/dev/lxcdev0", O_RDWR);
		if (-1 == fd)
		{
			perror("open error");
		}
		else
		{
			printf("open success\n");
			unsigned long fifo_size = 1;
			printf("fifo_size ptr = %p\n", &fifo_size);
			int result = ioctl(fd, LXC_IOCTL_GET_FIFO_LEN, &fifo_size);
			if (0 == result)
			{
				printf("current_size is %lu\n", fifo_size);
			}
			else
			{
				printf("get size error:%d\n", result);
			}
			close(fd);
		}
	}
	else
	{
		printf("unknown cmd:%s\n", cmd);
	}

	return 0;
}

void read_from_fd (int fd, char *buff, size_t buff_len)
{
	memset (buff, 0, buff_len);
	ssize_t ret = read(fd, buff, buff_len-1);
	if (-1 == ret)
	{
		perror("read data");
	}
	else if (ret > 0)
	{
		char * cc = NULL;
		for (size_t i = 0; i < ret; ++ i)
		{
			cc = buff + i;	
			*cc ^= 0x55;
		}

		printf("read data:%s\n", buff);
	}
	else if (ret == 0)
	{
		printf("there is no data now\n");
	}
}

int run_poll_reader(void)
{
	printf("start poll read\n");

	int fd = open("/dev/lxcdev0", O_RDWR);
	if (-1 == fd)
	{
		perror("open error");
		return 0;
	}
	else
	{
		printf("open success\n");
	}

	char buff[MAX_LENGTH + 1] = { 0 };

	pollfd fds[1];
	while (1)
	{
		fds[0].fd = fd;
		fds[0].events = POLLIN;
	
		int result = poll(fds, 1, -1);
		if (-1 == result)
		{
			printf("poll error, %d\n", errno);
			sleep(1);
			continue;
		}
		else if (0 == result)
		{
			printf("no data ready now\n");
			sleep(1);
			continue;
		}

		printf("data ready now\n");
		read_from_fd(fd, buff, MAX_LENGTH + 1);
	}

	close(fd);
	printf("poll_reader done\n");
	return 0;
}

int run_select_reader(void)
{
	printf("start select read\n");

	int fd = open("/dev/lxcdev0", O_RDWR);
	if (-1 == fd)
	{
		perror("open error");
		return 0;
	}
	else
	{
		printf("open success\n");
	}

	char buff[MAX_LENGTH + 1] = { 0 };

	while (1)
	{
		fd_set read_fdset;
		FD_ZERO(&read_fdset);
		FD_SET(fd, &read_fdset);

		struct timeval wait_time = { 0 };
		wait_time.tv_sec = 100;
		wait_time.tv_usec = 0;

		printf("start select now\n");
		int result = select(fd+1, &read_fdset, NULL, NULL, &wait_time);
		if (-1 == result)
		{
			printf("select ret -1, %d\n", errno);
			sleep(1);
			continue;
		}
		else if (0 == result)
		{
			printf("timeout, data not ready now\n");
			continue;
		}

		if (FD_ISSET(fd, &read_fdset))
		{
			printf("data ready now\n");
			read_from_fd(fd, buff, MAX_LENGTH + 1);
		}
		else
		{ 
			printf("select return, but not fd_set\n");
		}
	}

	close(fd);

	printf("end select read\n");
	return 0;
}

int main (int argc, char** argv)
{
	if (argc < 2)
	{
		printf("too less params\n");
		return 0;
	}

	if (strcmp (argv[1], "-w") == 0) // 写数据
	{
		return run_writer();
	}
	else if (strcmp(argv[1], "-r") == 0)
	{
		return run_reader(); // 普通读
	}
	else if (strcmp(argv[1], "-pr") == 0)
	{
		return run_poll_reader(); // poll 读
	}
	else if (strcmp(argv[1], "-sr") == 0)
	{
		return run_select_reader(); // select 读
	}
	else
	{
		return run_test(argv[1]);
	}
}

