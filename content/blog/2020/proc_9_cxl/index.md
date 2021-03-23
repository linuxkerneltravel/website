---
title: "第九期 《proc文件系统浅析》"
date: 2020-09-30T20:36:54+08:00
author: "无，陈小龙"
keywords: ["proc"]
categories : ["走进内核"]
banner : "img/blogimg/cxl_8.jpg"
summary : "proc文件系统是linux内核设计中一个经典的部分，她允许用户动态的查看内核的运行情况，包括当前系统中所有进程运行的信息，系统硬件信息， 内存使用情况等。现在许多软件都是通过proc文件系统提取内核的信息，例如ps，我们也可以通过proc文件系统动态的修改内核的一些配置而不必要重新 编译内核。所以我们有必要了解一下proc文件系统，以帮助我们更好的驾驭linux系统。"
---

proc文件系统是linux内核设计中一个经典的部分，她允许用户动态的查看内核的运行情况，包括当前系统中所有进程运行的信息，系统硬件信息， 内存使用情况等。现在许多软件都是通过proc文件系统提取内核的信息，例如ps，我们也可以通过proc文件系统动态的修改内核的一些配置而不必要重新 编译内核。所以我们有必要了解一下proc文件系统，以帮助我们更好的驾驭linux系统。下面是一些对proc文件系统的浅显的认识，希望对读者有帮 助： 

 1.[proc文件系统探索 之 以数字命名的目录

 包括对进程目录中cmdline文件的解析。

 2.[proc文件系统探索 之 以数字命名的目录

 包括对进程目录中cmd，environ，exe，maps，smaps文件的解析。

 3.[proc文件系统探索 之 以数字命名的目录

 包括对进程目录中fdinfo，root，stat文件的解析。 

4.[proc文件系统探索 之 以数字命名的目录

 包括对进程目录中statm，status，mounts，io文件的解析。

 5.[.](http://http//wwww.kerneltravel.net//?page_id=233)[proc文件系统探索 之 根目录下的文件

 包括对进程目录中lock,misc,moubles,partitions文件的解析。

 6.[.](http://http//wwww.kerneltravel.net//?page_id=240)[proc文件系统探索 之 根目录下的文件

 包括对proc根目录下stat,uptime,swaps三个文件的解析。 

7.[.proc文件系统探索 之 根目录下的文件

[七\]](http://http//wwww.kerneltravel.net//?p=278) 包括对proc根目录下meminfo文件的解析。[.](http://wwww.kerneltravel.net/index.php/buy-allegra-rx)http://wwww.kerneltravel.net/index.php/buy-allegra-rx)

