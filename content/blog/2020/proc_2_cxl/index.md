---
title: "proc文件系统探索 之 以数字命名的目录[二]"
date: 2020-09-29T21:21:15+08:00
author: "陈小龙转"
keywords: ["proc"]
categories : ["走进内核"]
banner : "img/blogimg/cxl_7.jpg"
summary : "cmd目录链接:该目录链接指向该进程运行的当前路径。该符号链接虽然使用ls命令查看其权限是对所有用户都有权限，但实际中是只有启动该进程的用户才具有读写的权限，其他用户不具有一切权限。该链接指向该进程运行的当前路径，例如我们在用户目录下启动该进程，那么cwd就指向用户目录。"
---

 3．cmd目录链接: 该目录链接指向该进程运行的当前路径。该符号链接虽然使用ls命令查看其权限是对所有用户都有权限，但实际中是只有启动该进程的用户才具有读写的权限，其他用户不具有一切权限。该链接指向该进程运行的当前路径，例如我们在用户目录下启动该进程，那么cwd就指向用户目录。

 4．environ文件: 包含该进程运行的环境变量。我们常用的一些环境变量都包含在该文件中，例如PATH,HOME,PWD等。所以如果我们想在一个进程中获取这些环境变量而又不想使用getenv(),getpwd函数的时候，我们就可以通过直接读取该进程的该文件以直接获得环境变量。下面是一个程序实现这个过程：

`#include<stdio.h>`

 #include<unistd.h>

 #include<string.h>

 #include<sys/types.h>

 #include <fcntl.h>

 int main(int args,char *argv[]) {

 FILE *fp; int pid; char path[80];

 int i=0; 

unsigned char buff[1024];

 unsigned char *p=NULL;

 unsigned char ch;

 if(args<2) { 

printf("no argument\n"); return 0; 

}

pid=getpid();

 snprintf(path,80,"/proc/%d/environ",pid);

 if((fp=fopen(path,"r"))==NULL) { 

perror("fopen"); return 0;

 }

 while(!feof(fp)) {

 if((ch=fgetc(fp))!='\0') 

{ buff[i]=ch; i++; continue; 

} 

buff[i]='\0';

 if((p=strstr(buff,argv[1]))!=NULL) {

 printf("%s\n",p+strlen(argv[1])+1);

 return 0;

 }

 i=0; 

memset(buff,'\0',1024);

 } fclose(fp);

 return 0; 

} 

运行过程：

niutao@niutao-desktop:~/c$ gcc -o getenv getenv.c niutao@niutao-desktop:~/c$ ./getenv HOME /home/niutao

5．exe链接文件: 指向该进程相应的可执行文件。

从这里我们可以看到该进程的可执行文件所在路径，例如：

 niutao@niutao-desktop:/proc/8070$ ls -l 

exe lrwxrwxrwx 1 niutao niutao 0 2008-10-21 22:01 exe -> /bin/bash 

niutao@niutao-desktop:/proc/8070$ 

我们可以看到pid等于8070的是一个bash程序，其所在路径为/bin。

 6．maps文件和smaps文件 maps 文件是可执行文件或者库文件对应的内存映像，而smaps文件显示的是该进程这些可执行文件或者库文件内存映像在内存中的大小等信息。在 GNOME桌面下的gnome-system-monitor中，我们选中一个进程，右键Memory Maps，其中显示的内容就来自这两个文件的信息。 首先我们来看maps文件:

```
niutao@niutao-desktop:/proc/6740$ cat maps 08048000-080ef000 r-xp 00000000 08:0d 636485 /bin/bash 080ef000-080f5000 rw-p 000a6000 08:0d 636485 /bin/bash 080f5000-08337000 rw-p 080f5000 00:00 0 [heap] b7c91000-b7cd0000 r--p 00000000 08:0d 1183449 /usr/lib/locale/en_US.utf8/LC_CTYPE b7cd0000-b7cd9000 r-xp 00000000 08:0d 1111788 /lib/tls/i686/cmov/libnss_files-2.7.so b7cd9000-b7cdb000 rw-p 00008000 08:0d 1111788 /lib/tls/i686/cmov/libnss_files-2.7.so b7cdb000-b7ce3000 r-xp 00000000 08:0d 1111790 /lib/tls/i686/cmov/libnss_nis-2.7.so b7ce3000-b7ce5000 rw-p 00007000 08:0d 1111790 /lib/tls/i686/cmov/libnss_nis-2.7.so 。。。。
```

我们可以看到pid等于6740的进程的内存映像。其中从0x08048000到0x080ef000是/bin/bash文件的内存映像，其在虚拟内存中 的偏移为0x00000000，该文件的inode为636485，其访问标志为r-xp，即可读，可执行，私有的。由此我们可以猜出此为/bin/bash可执行文件的程序段。而紧接着一个从0x80ef000到0x080f5000也是/bin/bash文件的内存映像，从其访问标志rw-p可 以看出其可读，可写，私有。我们推测其为数据段。08:0d表示文件所在的设备的主设备号和次设备号，比如这个08:0d，其中08"为主设备号，表示该 文件所在的设备是一个SCSI类型的硬盘(更详细的设备类型标号参见/linux/Documentation/devices.txt文件)，"0d" 表示次设备号为13，所以我们可以知道该文件/bin/bash的设备驱动是/dev/sda13。对于共享库，各共享库的数据段，存放着程序执行所需的全局变量，是由kernel把ELF文件的数据段map到虚存空间；用户代码段，存放着二进制形式的可执行的机器指令，是由kernel把ELF文件的代 码段map到虚存空间；用户数据段之上是代码段，存放着程序执行所需的全局变量，是由kernel把ELF文件的数据段map到虚存空间；用户数据段之下 是堆(heap)，当且仅当malloc调用时存在，是由kernel把匿名内存map到虚存空间，堆则在程序中没有调用malloc的情况下不存在；用 户数据段之下是栈(stack)，作为进程的临时数据区，是由kernel把匿名内存map到虚存空间，栈空间的增长方向是从高地址到低地址。maps文 件列出的内容基本都是这样的形式，即对一个被当前进程使用的文件都分别列出其程序段和数据段。 现在我们再来看一些smaps文件:

```
niutao@niutao-desktop:/proc/6740$ cat smaps 08048000-080ef000 r-xp 00000000 08:0d 636485 /bin/bash Size: 668 kB Rss: 584 kB Shared_Clean: 584 kB Shared_Dirty: 0 kB Private_Clean: 0 kB Private_Dirty: 0 kB Referenced: 584 kB 080ef000-080f5000 rw-p 000a6000 08:0d 636485 /bin/bash Size: 24 kB Rss: 24 kB Shared_Clean: 0 kB Shared_Dirty: 0 kB Private_Clean: 0 kB Private_Dirty: 24 kB Referenced: 24 kB 。。。。
```

我们可以看到其显示的信息比maps文件更详细。对于内存映像的每一个段它都列出该段的详细信息，如大小等。 所以我们可以结合这两个文件获取该进程的完整的内存映像信息。[.](http://wwww.kerneltravel.net/index.php/buy-gestanin-non-prescription)