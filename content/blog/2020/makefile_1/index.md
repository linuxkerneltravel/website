---
title: "在内核中新增驱动代码目录（1）"
date: 2020-06-20T22:25:36+08:00
author: "薛晓雯整理"
keywords: ["字符设备","驱动代码"]
categories : ["文件系统"]
banner : "img/blogimg/makefile1.png"
summary : "内容概要"
---

**Step by Step**

如果学习Linux下驱动开发，那么本文所述的“在内核中新增驱动代码目录”应该是一个最基本的知识点了。那么*如何将自己写好的驱动程序新增到内核*？本文将一步一步的教会你。

1.在正式开始之前，请先切换到root用户：su root。不过可能会会出现问题：不管你输入什么密码，都会提示你错误（很可能是因为之前你根本未设置过密码）。这时候我们来修改root用户的密码：

 sudo passwd root

输入两次后，即可修改完毕，这下再su root就可以成功切换到root用户。

2.你可以现在试着在终端输入make menuconfig，终端会提示你：make: *** 没有规则可以创建目标“menuconfig”。这是因为menuconfig涉及到图形界面，所以我们得安装一些依赖包（ubuntu下）：sudo apt-get install libncurses5-dev。

3.在一般的教程中，都会提到.config文件，而且这个文件就位于内核代码的根目录下。因此我会输入命令：ls -a来寻找.config。可是找来找去都没有这个文件的踪影。这是为什么？这是因为在这之前，你从来没有进行过内核配置，所以当然就不会生成.config文件了。解决的方法也很简单，有了上面两步的准备工作，那么你应该会成功进入配置用户界面，然后什么也不做，保存退出即可。那么你再ls一下，你可以发现.config已经存在了。

在开始向加入驱动代码之前，我们先了解三项基本步骤：

(1)将编好的源代码复制到Linux内核源代码的相应目录

(2)在目录的Kconfig文件中增加新源代码对应项目的编译配置选项

(3)在目录的Makefile文件中增加对新源代码的编译条目

在完成上述三项工作之前，我们先看一下我们要新增的驱动的树形结构。比如我们写的驱动程序均放在edsionteDriver目录，在此目录中包含Kconfig，Makefile和test.c三个文件，以及Key和led两个目录。我们先提前创建好这些文件，请注意本文只是为了演示说明，如果实际应用，像key，led以及test.c这样的文件都是有实际意义的。那么现在复制到内核源码目录下的driver/目录下即可。

| – edsionteDriver  
|　|– Kconfig  
|　|– Makefile  
|　|– key  
|　　|– Kconfig  
|　　|– Makefile  
|　　|– mykey.c  
|　|– mydriver.c  
|　|– mydriver_user.c  

现在我们完成了第一步工作，你应该注意到，我们现在只是创建了各个目录下的Kconfig和Makefile文件，并没添加相关内容，所以接下来我们就来进行这两个文件的编写。

对于初学者来说，直接学习Makefile以及Kconfig的编写可能会有些眩晕甚至排斥学习，不过我们可以先了解这两个文件在实际的内核分析中有什么作用。一般来说，对于内核这个庞大的网络，想要快速定位你所关心的代码就需要首先分析某个目录下的Makefile以及Kconfig文件，它们可是我们分析内核代码的goole map。

比如我们要分析ext3类型的文件系统，那么我们进入源码目录下fs/ext3/目录中，我们打开此目录的Makefile文件：


```c
1 #
2 # Makefile ``for` `the linux ext3-filesystem routines.
3 #
4
5 obj-$(CONFIG_EXT3_FS) += ext3.o
6  
7 ext3-y  := balloc.o bitmap.o dir.o file.o fsync.o ialloc.o inode.o \
8            ioctl.o namei.o super.o symlink.o hash.o resize.o ext3_jbd.o
9
10 ext3-$(CONFIG_EXT3_FS_XATTR)     += xattr.o xattr_user.o xattr_trusted.o
11 ext3-$(CONFIG_EXT3_FS_POSIX_ACL) += acl.o
12 ext3-$(CONFIG_EXT3_FS_SECURITY)  += xattr_security.o
```

我们应该先注意到7，8行，其定义了ext3变量（-y说明是多文件模块的定义，可以先忽略）。这里的定义变量类似于C语言中的宏定义，就是用ext3代替后面的.o文件列表。那么现在我们就可以知道与ext3模块最直接相关的就是后面这些文件对应的.c以及.h文件了，这些文件在源码相应的目录下都可以找到。那么ext3.o是否被编译取决于第的CONFIG_EXT3_FS，这个变量的值一般取y或n（甚至m）。它一般对应的是用户在配置界面时的输入。想要了解配置界面的菜单选项，就得看Kconfig文件。由于我们只关心EXT3_FS这个选项，因此我们相应的找到EXT3_FS这个选项的配置语句即可：


```c
config EXT3_FS  
　　tristate "Ext3 journalling file system support"  
　　select JBD     
　　help  
　　This is the journalling version of the Second extended file system      
　　(often called ext3), the de facto standard Linux file system  
　　(method to organize files on a storage device) for hard disks.   
　　　#Other code was deleted
```


上述代码中，select说明只有JBD被配置，EXT3_FS这个配置项目才会在配置菜单上出现（事实上两者有更具体的依赖关系，可参考相关语法）。在配置菜单上会显示tristate后面的字符串，当用户选择配置此条目的情况下（有y，m和n三态选项），对应在Makefile文件中的CONFIG_EXT3_FS就对应为y。即obj-y的情况下，ext3.o菜会被编译。

现在将Makefile和Kconfig文件再串通起来想想，你应该会明白它们的作用。

一般来说，Makefile定义了根据该子目录下的源码文件构建目标文件的规则。像我们刚说的那个变量的定义以及根据CONFIG_EXT3_FS选项是否编译ext3.c文件。至于这些规则是否被执行，就取决于用户在配置菜单上是否选择配置这个选项，而这个配置菜单中配置选项就对应Kconfig文件。而且用户输入的配置结果会记录在.config文件当中。

通过上面的简单举例，我们可以先大致了解Makefile、Kconfig以及.config三者之间的关系以及作用。我们刚才分析的是内核中已经写好的代码的配置规则。对于我们上面所说的新增edsionteDriver驱动，应该如何添加？

具体添加过程请参见：在内核中新增驱动代码目录(2)。