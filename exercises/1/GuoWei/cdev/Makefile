obj-m+=zfchar.o

all:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) modules
	$(CC) zfread.c -o zfread
	$(CC) zfwrite.c -o zfwrite
clean:
	make -C /lib/modules/$(shell uname -r)/build/ M=$(PWD) clean
	rm zfread zfwrite
