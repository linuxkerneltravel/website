---
title : "电子杂志"
date : "2020-05-29T13:47:08+02:00"
---

[第一期《走入Linux 世界》](http://wwww.kerneltravel.net/journal/i/index.htm)

摘要：本期涉猎了操作系统的来龙去脉后与大家携手步入 Linux 世界。我们力图展示给大家一幅 Linux 系统的全景图，并为了加深对 linux 系统的全面认知，亲手搭建了一个能运行在内存中的试验系统。同时为大家提供了几个 shell 脚本帮助建立试验系统。

[第二期《i386 体系结构》（上）](http://wwww.kerneltravel.net/journal/ii/index.htm)

摘要：本期上半部分将和网友一起聊聊 I386 体系结构，认识一下 Intel 系统中的内存寻址和虚拟内存的来龙去脉。下半部分将实现一个最最短小的可启动内核，一是加深对 i386 体系的了解，再就是演示系统开发的原始过程。

[第二期《i386 体系结构》（下）](http://wwww.kerneltravel.net/journal/ii/index.htm)

摘要： 上半期我们一起学习了 I386 体系结构，下半期我们的主要目标是实现一个能启动而且可以进入保护模式的简易操作系统。所以本期首先来分析一下计算机的启动流程，然后着手学习开发一个基 于 I386 体系的可启动系统。

[第三期《编写自己的Shell 解释器》](http://wwww.kerneltravel.net/journal/iii/index.htm)

摘要： 本期的目的是向大家介绍 shell 的概念和基本原理，并且在此基础上动手做一个简单 shell 解释器。同时，还将就用到的一些 linux 环境编程的知识做一定讲解。

[第四期《Linux 系统调用》](http://wwww.kerneltravel.net/journal/iv/index.htm)

摘要：本期重点和大家讨论系统调用机制。其中涉及到了一些及系统调用的性能、上下文深层问题，同时也穿插着讲述了一些内核调试方法。并 且最后试验部分我们利用系统调用与相关内核服务完成了一个搜集系统调用序列的特定任务，该试验具有较强的实用和教学价值。

[第五期《Linux 内存管理》](http://kerneltravel.net/blog/2020/memory_management/)

摘要： 本章首先以应用程序开发者的角度审视 Linux 的进程内存管理，在此基础上逐步深入到内核中讨论系统物理内存管理和内核内存地使用方法。力求从外自内、水到渠成地引导网友分析 Linux 地内存管理与使用。在本章最后我们给出一个内存映射地实例，帮助网友们理解内核内存管理与用户内存管理之间地关系，希望大家最终能驾驭 Linux 内存管理。

[第六期《 ](http://wwww.kerneltravel.net/journal/vi/index.htm)[内核中的调度与同步 ](http://wwww.kerneltravel.net/journal/vi/index.htm)[》](http://wwww.kerneltravel.net/journal/v/index.htm)

摘要 ：本章将为大家介绍内核中存在的各种任务调度机理以及它们之间的逻辑关系（这里将覆盖进程调度、推后执行、中断等概念、内核线程），在此基础上向大家解释 内核中需要同步保护的根本原因和保护方法。最后提供一个内核共享链表同步访问的例子，帮助大家理解内核编程中的同步问题。

[第七期《如何实现Linux 下的文 件系统》](http://wwww.kerneltravel.net/journal/vii/index.htm)

摘要 : 本章目的是分析在 Linux 系统中如何实现新的文件系统。在介绍文件系统具体实现前先介绍文件系统的概念和作用，抽象出了文件系统概念模型。熟悉文件系统的内涵后，我们再近一步讨论 Linux 系统中和文件系统的特殊风格和具体文件系统在 Linux 中组成结构，逐步为读者勾画出 Linux 中文件系统工作的全景图。最后在事例部分，我们将以 romfs 文件系统作实例分析实现文件系统的普遍步骤。

[第八期《中断》](http://wwww.kerneltravel.net/journal/viii/index.htm)

摘要 : 本章将向读者依次解释中断概念，解析 Linux 中的中断实现机理以及 Linux 下中断如何被使用。作为实例我们将向第二期中打造的系统中加入一个时钟中断，希望可以帮助读者掌握中断相关的概念和编程方法。