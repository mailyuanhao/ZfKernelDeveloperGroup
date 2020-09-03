

## 功能

1. 对信息使用0x55进行异或加密的字符设备驱动，同时编写两个应用层进程。实现如下操作：
+ A进程用于产生随机信息，并写入驱动模块。
+ B进程从驱动中读取加密后的数据并输出。
+ B必须按照A写入信息的顺序进行打印。A和B之间不能进行其他同步操作。
+ 驱动可以缓存未被及时读取的信息。

## 实现
+ zfchar.c 实现一个字符设备对信息进行0x55异或加密
+ zfwrite.c 打开字符设备一直往字符设备写入随机int型数据
+ zfread.c 打开字符设备一直读取设备中内容打印
+ Makefile 编译文件
+ 99-zfchar.rules 字符设备的规则文件

## 运行
```shell
# make clean
# make
# cp 99-zfchar.rules /etc/udev/rules.d/
# insmod zfchar.ko
# ./zfwrite
# ./zfread
# rmmod zfchar.ko
```