---
title: "直播复盘和回放 | 梦想起航-自研操作系统BOOKOS之旅"
date: 2021-03-22T10:00:45+08:00
author: "作者：BookOS胡自成 编辑：张玉哲"
keywords: ["社区"]
categories : ["社区"]
banner : "img/blogimg/BOOKOS.jpg"
summary : "直播复盘 | 梦想起航-自研操作系统BOOKOS之旅，文章末尾可以下载ppt。"
---

在上次的直播里，我们介绍BookOS的来源与其发展，并分析了其内核xbook2的框架，以及网络服务框架和图形框架。直播也收到很多观众的关注与喜欢，今天我们来总结一下上次的直播内容！
 1. BookOS的来源与发展
 2. BookOS的环境构建以及演示
 3. BookOS的网络服务框架
 4. BookOS的图形框架


## 1、BookOS的来源与发展

该部分介绍了BookOS的名字来源，来自高尔基名言：“书是人类进步的阶梯”。并且讲了发展史，BookOS也算是有历史的OS了，虽然很短暂。

<img src="img/1.png" style="zoom:85%">


## 2、 BookOS的环境构建以及演示

该环节演示了如何搭建开发环境，以及编译源码，并对BookOS和xbook2进行虚拟机演示和物理机演示。

<img src="img/2.png" style="zoom:85%">

xbook2内核演示

<img src="img/3.png" style="zoom:85%">

BookOS演示


## 3、BookOS的网络服务框架

在这部分主要介绍了BookOS的网络服务框架，融入了微内核的思想，基于lwip协议栈和lpc机制来实现，是一个位于用户态的服务程序。这种模式既有优点，又有缺点，只能算是一种大胆的尝试。

<img src="img/4.png" style="zoom:85%">


## 4、BookOS的图形框架

在这一部分，讲解了BookOS的图形框架，view图形驱动，是位于内核态的驱动程序，这个驱动又基于鼠标、键盘、视频驱动，相当于一个抽象的驱动。uveiw图形库，就是对这个驱动接口的简单封装。在此基础上，有一个xtk图形库，类似于gtk图形库，在此基础上构建起了BookOS的图形界面。

并且还和xwindow以及wayland进行了比较分析，来证明这个框架的可行性。

<img src="img/5.png" style="zoom:85%">


## 5、操作系统开发资料推荐

**书籍：**


一步一步带你写系统的书籍：

《x86汇编从实模式到保护模式》--李忠、王晓波、余洁

《orange’s一个操作系统的设计与实现》--于渊

《操作系统真相还原》--郑刚

《30天自制操作系统》--川和秀实

《一个64位操作系统的设计与实现》--田宇

《深度探索嵌入式操作系统》--彭东


源码分析书籍：

《linux内核设计与实现》-Robert Love著，陈莉君、康华译

《操作系统设计与实现》--Andrew S. Tanenbaum

《Linux内核源代码情景分析》--毛德操、胡希明

《操作系统设计Xinu方法》--Douglas Comer

《深入理解Linux虚拟内存管理》--Mel Gorman

《深入linux内核架构》--Wolfgang Mauerer

《Linux内核完全剖析：基于0.12内核》--赵炯

《嵌入式网络那些事——STM32物联实战-朱升林-2015年版》

《Windows内核原理与实现》


**网站：**


osdev操作系统开发爱好者必备：wiki.osdev.org

bookos官网：www.book-os.org

csdn孤舟钓客-babyos2，babyos


**项目：**


linux内核项目：https://github.com/torvalds/linux

minix3内核项目：

https://github.com/Stichting-MINIX-Research-Foundation/minix

xv6：https://github.com/mit-pdos/xv6-public

清华大学ucore：https://github.com/chyyuu/ucore_os_lab

清华大学rcore：https://github.com/rcore-os/rCore

https://github.com/xboot/xboot

https://github.com/klange/toaruos

https://github.com/SerenityOS/serenity

https://github.com/dbittman/seakernel
 
国产64位系统内核MINEOS：

https://gitee.com/MINEOS_admin/OS-Virtual-Platform

国产32位操作系统内核xbook2：https://github.com/hzcx998/xbook2

国产32位操作系统BookOS：https://github.com/hzcx998/BookOS


## 总结

BookOS的介绍直播只是简单地说了一下整体框架，让大家对内核的框架以及当前实现的功能有所了解。直播的最后也推荐了一些学习操作系统开发的书籍和资料，相信对想自己开发操作系统的同学一定会有所帮助！

[点击下载ppt](BookOS.pdf)
