---
title: "ubuntu下2.6.24内核编译升级"
date: 2020-09-20T17:10:36+08:00
author: "作者helight0,陈小龙整理"
keywords: ["ubuntu","内核升级"]
categories : ["新手上路"]
banner : "img/blogimg/cxl_5.jpg"
summary : "本文主要介绍了ubuntu内核的升级，包括一下四个步骤，分别是安装必要的工具包，查看ubuntu的内核版本，下载需要升级到的的内核版本，开始编译等过程"
---

第一步 安装必要的工具

首先要安装必要的包。 包有：libncurses5-dev（menuconfig需要的）和essential sudo apt-get install build-essential kernel-package sudo apt-get install make sudo apt-get install gcc 另外，查看系统是否有这样的两个命令 mkinitramfs mkisofs 这两个工具在编译内核时用来生成 *.img文件的。如果没有就需安装。

 第二步 下载内核 到www.kernel.org下载新内核到/usr/src 我下载的是linux2.6.24.tar.gz(原来的内核是2.6.20-15-generic)

第三步 编译前的准备 察看当前内核的版本 helight@helight-desktop:/$ uname -a

Linux helight-desktop 2.6.20-15-generic #1 SMP Mon Aug 25 17:32:09 UTC 2008 i686 GNU/Linux

helight@helight-desktop:/$

 建议最好下载比当前已安装版本高的内核 解压linux-2.6.24.tar.gz到linux-2.6.24 

cd /usr/src

sudo tar zxvf linux-2.6.24.tar

cd linux-2.6.24/

第四步 开始编译 

cd /usr/src/linux-2.6.24 //以下所有的工作都在/usr/src/linux-2.6.24下完成 sudo make menuconfig  //用menuconfig的话还需要Ncurses，或者用 sudo make xconfig

sudo make menuconfig //一般是用menuconfig

 配置完以后保存（系统中保存的一份内核配置文件是在/usr/src/linux-2.6.24下名为.config,你也可以自己在别的地方另存一份） 

也可以cp原来在/boot目录下的config-2.6.xx 到当前目录下，在make menuconfig是使用这个配置文件。

 sudo make dep   //也许系统会提示现在不必要进行make dep，那就下一步 2.6.24的我编译就没有使用过。

 sudo make clean //清除旧数据 ，新解压的内核源码就不需要这一步了

sudo make –j2 可以分两个线程来进行编译工作，不过我用make –j4 却发现系统有9个make进程在工作。所以这个参数未必起作用。

 sudo make bzImage //编译内核，将保存到/usr/src/linux-2.6.24/arch/i386/boot/下 sudo make modules //编译模块 sudo make modules_install //安装模块 sudo mkinitramfs -o /boot/initrd-2.6.24.img 2.6.24

此时可能提示找不到这样的一个文件夹“/lib/firmware/2.6.24”，你需要手工创建一个这样的文件夹。

sudo mkdir /lib/firmware/2.6.24 sudo make install //安装内核 安装完后/boot下将增加以下几个文件（用ls -l *24*查看）

helight@helight-desktop:/boot$ ls -l *24*

-rw-r--r-- 1 root root  85203 2008-03-14 22:24 config-2.6.24

-rw-r--r-- 1 root root  85203 2008-03-14 20:23 config-2.6.24.old

-rw-r--r-- 1 root root 37968871 2008-03-15 08:31 initrd-2.6.24.img

-rw-r--r-- 1 root root 4014080 2008-03-14 22:24 initrd.img-2.6.24

-rw-r--r-- 1 root root  932315 2008-03-14 22:24 System.map-2.6.24

-rw-r--r-- 1 root root  932315 2008-03-14 20:23 System.map-2.6.24.old

-rw-r--r-- 1 root root 1858864 2008-03-14 22:24 vmlinuz-2.6.24

-rw-r--r-- 1 root root 1858864 2008-03-14 20:23 vmlinuz-2.6.24.old

helight@helight-desktop:/boot$

给/boot/grub/menu.lst中添加一个新的启动项，如我的menu.lst增加了如下一段文字

title       Ubuntu, kernel 2.6.24

root      (hd0,0)

kernel     /boot/vmlinuz-2.6.24root=UUID=d7e2cf74-ebf5-4c78-ac2c-9f85a9809eae ro

initrd     /boot/initrd-2.6.24.img

从新启动即可。[.](http://wwww.kerneltravel.net/index.php/mobic-online-coupon-code)