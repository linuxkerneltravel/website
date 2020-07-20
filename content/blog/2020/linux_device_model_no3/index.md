---
title: "Linux设备驱动模型（三）-追根之溯源"
date: 2020-07-19T18:54:01+08:00
author: "编辑：戴君毅"
keywords: ["设备模型"]
categories : ["文件系统"]
banner : "img/blogimg/filesystem.jpeg"
summary : "软件设计的根本是把现实世界的事物用计算机世界的模型表示出来，Linux设备模型的设计采用了面向对象（Object Oriented）的思想。"
---

软件设计的根本是把现实世界的事物用计算机世界的模型表示出来，Linux设备模型的设计采用了**面向对象**（Object Oriented）的思想。

在前一讲中，提到sysfs文件系统，sysfs文件系统的目标就是要展现设备驱动模型组件之间的**层次关系**。在Linux中，sysfs文件系统被安装于/sys目录下,见上图一。那么，在这样的目录树中，哪些目录是驱动模型要关注的对象？

> ***bus***-系统中用于连接设备的总线，在内核中对应的结构体为**struct bus_type {… }**;
>
> ***device***-内核所识别的所有设备，依照连接它们的总线对其进行组织,对应的结构体为***struct device{… };***
>
> ***class***-系统中设备的类型（声卡，网卡，显卡，输入设备等），同一类中包含的设备可能连接到不同的总线,对应的结构体为***struct class{… };***

为什么不对Power进行单独描述？实际上，Power与device有关，它只是device中的一个**字段**。

除此之外，立马闪现在我们脑子里的对象还有：***driver***-在内核中注册的设备驱动程序，对应的结构体为***struct device_driver\{… };***

以上bus，device，class，driver是可以感受到的对象，在内核中都用相应的结构体来描述。而实际上，按照面向对象的思想，我们需要抽象出一个最基本的对象，这就是设备模型的核心对象**kobject**。

kobject是Linux 2.6引入的新的设备管理机制，在内核中就是一个struct kobject结构体。有了这个数据结构，内核中所有设备在底层都具有统一的接口，kobject提供基本的对象管理，是构成Linux2.6设备模型的核心结构，它与sysfs文件系统紧密关联，每个在内核中注册的kobject对象都对应于sysfs文件系统中的一个**目录**。kobject是组成设备模型的**基本结构**。类似于C++中的基类，好比MFC中的CObject、Java中的Object和COM中的IUnKnown。kobject嵌入于更大的对象中，即所谓的**容器**，如上面提到的bus,class，devices，drivers都是典型的容器，它们是描述设备模型的**组件**。