字符设备驱动实验，实现一个简单的字符设备模块代码。
允许一个进程写入随机字符串，内核中加密存储。另外程序读取加密数据，并还原数据。
模块初始化时可自动创建设备，不用手动加载，应用程序可直接打开/dev/lxcdev0，进行访问。
目前实现open/read/write/ioctl/poll/lseek/release等内核代码实现。

ko文件夹：
  lxcdev.c：内核模块实现文件
  Makefile
app文件夹：
  app.cpp: 应用层测试代码
  Makefile

更新日志：
2020-09-05：内核模块增加poll实现。测试程序增加select/poll两种方式读取数据。
2020-08-26：内核模块实现open/read/write/ioctl。
