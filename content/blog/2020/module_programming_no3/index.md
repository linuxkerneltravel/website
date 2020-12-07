---
title: "内核模块编程之入门（三）-模块实用程序简介"
date: 2008-11-09T16:50:24+08:00
author: "helight0"
keywords: ["内核模块"]
categories : ["走进内核"]
banner : "img/blogimg/2.png"
summary : "modutils是管理内核模块的一个软件包。可以在任何获得内核源代码的地方获取Modutils(modutils-x.y.z.tar.gz)源代码，然后选择最高级别的patch.x.y.z等于或小于当前的内核版本，安装后在/sbin目录下就会有insomod、rmmod、ksyms、lsmod、modprobe等实用程序。当然，通常我们在加载Linux内核时，modutils已经被载入。"
---

 1．Insmod命令 

​		调用insmod程序把需要插入的模块以目标代码的形式插入到内核中。在插入的时候，insmod自动调用init_module()函数运行。

​		注意，只有超级用户才能使用这个命令，其命令格式为： 

```shell
# insmod [path] modulename.c
```

2. rmmod命令 

   调用rmmod程序将已经插入内核的模块从内核中移出，rmmod会自动运行cleanup_module()函数，其命令格式为：

```shell
 #rmmod [path] modulename.c
```

 3．lsmod命令

​		调用lsmod程序将显示当前系统中正在使用的模块信息。实际上这个程序的功能就是读取/proc文件系统中的文件/proc/modules中的信息，其命令格式为：

```shell
 #lsmod 
```

4．ksyms命令 

​		ksyms这个程序用来显示内核符号和模块符号表的信息。与lsmod相似，它的功能是读取/proc文件系统中的另一个文件/proc/kallsyms。

