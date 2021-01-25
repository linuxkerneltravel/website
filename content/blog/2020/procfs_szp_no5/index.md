---
title: "proc文件系统探索 之 proc根目录下的文件[五]"
date: 2020-12-19T10:21:15+08:00
author: "孙张品转"
keywords: ["proc"]
categories : ["走进内核"]
banner : "img/blogimg/cxl_10.jpg"
summary : "内核锁，记录与被打开的文件有关的锁信息。该文件显示当前被内核锁定的文件。该文件包含的内容是内核调试数据，根据使用的系统的这些数据会变化很大。"
---
## 2.1 根目录下的文件 

### 2.1.1 lock文件

内核锁，记录与被打开的文件有关的锁信息。 该文件显示当前被内核锁定的文件。该文件包含的内容是内核调试数据，根据使用的系统的这些数据会变化很大。一个/proc/locks文件会和下面的相似：
```
niutao@niutao-desktop:/proc$ cat locks 1: POSIX ADVISORY READ 12944 08:0f::48897 1073741826 1073742335 2: POSIX ADVISORY WRITE 12944 08:0f::48881 1073741824 1073742335 3: POSIX ADVISORY WRITE 12944 08:0f::48876 0 EOF 4: POSIX ADVISORY WRITE 5938 08:0d:1085298 0 EOF 5: FLOCK ADVISORY WRITE 5817 00:11:15003 0 EOF 6: POSIX ADVISORY WRITE 5804 00:11:14995 0 EOF 7: POSIX ADVISORY READ 5489 08:0d:179581 4 4 8: POSIX ADVISORY READ 5539 08:0d:179581 4 4 9: POSIX ADVISORY READ 5489 00:11:13966 4 4 10: POSIX ADVISORY WRITE 5489 00:11:13965 0 0 11: POSIX ADVISORY WRITE 5208 08:0d:296757 0 EOF 12: POSIX ADVISORY WRITE 5208 08:0d:296756 0 EOF 13: POSIX ADVISORY WRITE 5208 08:0d:296755 0 EOF
```
每个锁都处于以一个唯一的数字开头的一行里。第二列表示使用该锁的对象，FLOCKS表示从一个flocksystem调用打开的早期风格的UNIX文件锁，POSIX表示从一个lockfsystem调用打开的新的POSIX锁。 第 三列有两个可取的值：ADVISORY或者MANDATORY。ADVISORY表示该锁不阻止其他进程访问被锁定的数据，它只是阻止企图锁定它的其他进 程。MANDATORY表示当锁被锁定的时候不允许访问被许可的数据。第四列表示该锁是否允许锁的持有者读或写被锁定的文件。第五列显示了持有该锁的进程 的id。第六列显示了被锁定的文件的id，格式是： 主设备号：次设备号：inode节点号 第七列和第八列分别表示文件锁锁定的区域的开始和结束。 
### 2.1.2 misc文件 

杂项设备信息。
```
niutao@niutao-desktop:/proc$ cat misc 63 vboxdrv 229 fuse 1 psaux 228 hpet 135 rtc 231 snapshot
```
该文件列出了系统在杂项主设备号（主设备号为10）上注册的设备。第一列表示该设备的次设备号，第二列显示的是该设备的名称。 

### 2.1.3 moubles文件 

系统正在使用的模块信息。 该文件显示的是加载进内核的所有模块。其内容根据不同的配置和你使用的系统的不同而不同，但基本和所示的/proc/modules文件一样：
```
binfmt_misc 12808 1 - Live 0xf8d0e000 rfcomm 41744 2 - Live 0xf8d2d000 l2cap 25728 13 rfcomm, Live 0xf8cbe000 bluetooth 61156 4 rfcomm,l2cap, Live 0xf8d1d000 vboxdrv 61360 0 - Live 0xf8cae000 nfsd 228848 13 - Live 0xf8d44000 lockd 67720 2 nfsd, Live 0xf8cf7000 nfs_acl 4608 1 nfsd, Live 0xf8c95000 auth_rpcgss 43424 1 nfsd, Live 0xf8ca2000
```

第一列包含该模块的名字，第二列表示该模块的内存大小，单位为字节。第三列列出了该模块当前有多少被加载的实例，如果为 0则表示该模块可以卸载。第四列列出了当前该模块需要哪些其他模块。第五列表示模块的加载状态：Live表示加载，其他值表示没有加载。第六列表示已经加 载的模块在内存中的偏移。这列信息对于调试是非常有用的。 

### 2.1.4 mtrr文件 
该文件指的是当前系统使用的内存类型范围寄存器（MTRRs）。如果当前的系统架构支持MTRRs，那么/proc/mtrr文件就会如下所示：
```
niutao@niutao-desktop:/proc$ cat mtrr reg00: base=0x00000000 ( 0MB), size=1024MB: write-back, count=1 reg01: base=0x3ff00000 (1023MB), size= 1MB: uncachable, count=1

```

MTRRs 被用在英特尔六位系列处理器（奔腾II和更高的）和控制处理器，内存访问范围。当在PCI或AGP总线上使用视频卡时，一个配置正确的/proc/ mtrr文件可以提高超过150%的性能 。大多数时候，这个值是正确设定的预设值。关于手动配置该文件的更多信息可以在内核文档中找到（/Documentation/mtrr.txt） 

### 2.1.5 partitions文件 

该文件包含硬盘分区信息。该文件的内容如下：
niutao@niutao-desktop:/proc$ cat partitions major minor #blocks name 8 0 312571224 sda 8 1 15727603 sda1 8 2 1 sda2 8 5 20972826 sda5 8 6 20972826 sda6 8 7 31455238 sda7 8 8 20972826 sda8 8 9 20972826 sda9 8 10 83883366 sda10 8 11 31455238 sda11 8 12 10482381 sda12 8 13 20972826 sda13 8 14 2096451 sda14 8 15 12586896 sda15

第一列表示主设备号，我们看到上面所示的全部是8，所以我们可以知道该系统使用的是一个SCSI硬盘（详见/Documentation/devices.txt）。第二列表示次设备号，第三列表示分区所占的物理块的个数，第四列表示分区的名字。
