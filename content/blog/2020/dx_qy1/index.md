---
title: "内核模块编程之入门（一）-话说模块"
date: 2020-12-05T09:00:22+08:00
author: "helight0"
keywords: ["内核模块","模块编程"]
categories : ["内核模块"]
banner : "img/blogimg/x86.png"
summary : "模块通常由一组函数和数据结构组成，用来实现一种文件系统、一个驱动程序或其他内核上层的功能。本文将实现一个简单的hello world内核模块作为入门教程"
---

内核模块是Linux内核向外部提供的一个插口，其全称为动态可加载内核模块（Loadable Kernel Module，LKM），我们简称为**模块**。Linux内核之所以提供模块机制，是因为它本身是一个单内核（monolithic kernel）。单内核的最大优点是效率高，因为所有的内容都集成在一起，但其缺点是可扩展性和可维护性相对较差，模块机制就是为了弥补这一缺陷。

一、 什么是模块

模块是具有独立功能的程序，它可以被单独编译，但不能独立运行。它在运行时被链接到内核作为内核的一部分在内核空间运行，这与运行在用户空间的进程是不同的。模块通常由一组函数和数据结构组成，用来实现一种文件系统、一个驱动程序或其他内核上层的功能。

二、 编写一个简单的模块

模块和内核都在内核空间运行，模块编程在一定意义上说就是内核编程。因为内核版本的每次变化，其中的某些函数名也会相应地发生变化，因此模块编程与内核版本密切相关。以下例子针对2.6内核

1．程序举例 hellomod.c 

```c
001 // hello world driver for Linux 2.6 
    
    
004 #include <linux/module.h> 

005 #include <linux/kernel.h>

006 #include <linux/init.h> /* 必要的头文件*/ 
    
    

009 static int __init lkp_init(void){ 

printk("<1>Hello,World! from the kernel space...\n"); 
return 0;
 }

015 static void __exit lkp_cleanup( void ) {

 printk("<1>Goodbye, World! leaving kernel space...\n"); 
 } 

020 module_init(lkp_init); 

021 module_exit(lkp_cleanup); 

022 MODULE_LICENSE("GPL");
```

．说明  
第4行：
        所有模块都要使用头文件module.h，此文件必须包含进来。
第5行：
        头文件kernel.h包含了常用的内核函数。
第6行：
        头文件init.h包含了宏_init和_exit，它们允许释放内核占用的内存。
建议浏览一下该文件中的代码和注释。
第9-12行：
        这是模块的初始化函数，它必需包含诸如要编译的代码、初始化数据结构等内容。
第11行使用了printk()函数，该函数是由内核定义的，功能与C库中的printf()类似，
它把要打印的信息输出到终端或系统日志。字符串中的<1>是输出的级别，
表示立即在终端输出。 
第15-18行：
        这是模块的退出和清理函数。此处可以做所有终止该驱动程序时相关的清理工作。
第20行：
        这是驱动程序初始化的入口点。对于内置模块，内核在引导时调用该入口点；
对于可加载模块则在该模块插入内核时才调用。
第21行：
        对于可加载模块，内核在此处调用module_cleanup（）函数，而对于内置的模块，
它什么都不做。
第22行：
  提示可能没有GNU公共许可证。有几个宏是在2.4版的内核中才开发的（详情参见modules.h）。
  函数module_init()和cleanup_exit()是模块编程中最基本也是必须的两个函数。
  module_init()向内核注册模块所提供的新功能，
  而cleanup_exit()注销由模块提供的所有功能。

