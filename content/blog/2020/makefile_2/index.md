---
title: "在内核中新增驱动代码目录（2）"
date: 2020-06-28T11:06:02+08:00
author: "薛晓雯编辑"
keywords: ["字符设备","驱动代码"]
categories : ["文件系统"]
banner : "img/blogimg/makefile1.png"
summary : "上文中，我们已经理解了Makefile与Kconfig的作用，那么我们现在要在内核中增加edsionteDriver驱动代码，并告诉内核“请您下次编译的时候捎带上我”。具体应该如何来做？首先应该在Makefile中添加相关驱动文件的编译信息，然后还得在Kconfig中添加这个新驱动对应的配置选项。"
---

上文中，我们已经理解了Makefile与Kconfig的作用，那么我们现在要在内核中增加edsionteDriver驱动代码，并告诉内核“请您下次编译的时候捎带上我”。具体应该如何来做？首先应该在Makefile中添加相关驱动文件的编译信息，然后还得在Kconfig中添加这个新驱动对应的配置选项。

好了，开始吧！

我们首先在内核根目录下添加edsioteDriver驱动目录。如何将此驱动目录与上一级目录连接起来？那么就需要我们来做一些修改。在驱动目录的上一级目录中，我们找到Makefile文件，然后在在此文件中添加下面代码(最好在自己添加的代码前后加上注释，以便与原有代码区别)：

```c
#edsionteDriver 's starting 
obj-y+=edsionteDriver/ 
#edsinteDriver's ending
```

这句话使得kbulid会将edsionteDriver目录列入到向下迭代的目标当中，通俗一点说就是在编译过程中会将我们写的这个驱动目录列入编译的范围之内。obj-y文件列表将会被链接到built-in.o，并最终编译到vmlinux的目标文件列表 。这个解释属于官方说法，事实上当前我也不是很清除这是神马意思，不过你只需了解obj-y开始的文件列表会被编译并连接至内核就可以了。

我们上文中所说obj-$(CONFIG_xxx):=(文件列表)这样的语句是定义变量，其中具体规则是：变量名 变量符　变量值。变量符可以是=，+=，:=，?=中的一个，其中+=是追加赋值符，实现对变量的追加赋值。其中$(CONFIG_XXX)是对CONFIG_XXX的引用，CONFIG_XXX的值会根据依赖关系以及用户输入来决定。比如如果是depend on依赖的话，那么用户可选择y或n；如果是上文中的tristate那么就是三态选项，即y、n或m。obj-m表示会将文件当作内核模块来编译，obj-n就可以忽略不进行处理。

注意：我们刚添加的语句只是对edsionteDriver目录起作用，至于这个目录下的各个子文件是要模块编译还是链入你和还要具体看我们edsionteDriver目录下的Makefile文件。

下面修改驱动目录子目录的Kconfig文件：

```c
#edsionteDriver 's starting 
source "drivers/edsionteDriver/Kconfig" 
#edsinteDriver's ending
```

这条脚本是将edsionteDriver目录中的Kconfig加入到上级目录中的Kconfig中。这样上级目录的Kconfig会引用到edsionteDriver目录中的Kconfig文件。通过这条语句我们也可以体会Kconfig的另一个作用：Kconfig文件将分布在当前目录下各个子目录的Kconfig都集中在自己的Kconfig当中。

通过上述两步修改就会让父目录感知到edsionteDriver的存在了。接下来我们来编写edsionteDriver目录以及其子目录下的各个Kconfig和Makefile文件。首先是edsionteDriver目录下的两个文件配置：

Makefile文件如下：

```c
# drivers/edsionteDriver/Makefile 
# just a test 

obj-$(CONFIG_MYDRIVER) +=mydriver.o 
obj-$(CONFIG_MYDRIVER_USER) +=mydriver_user.o 
obj-$(CONFIG_KEY) +=key/
```

我们需要用户来选择是否编译mydriver.c，所以需要用CONFIG_MYDRIVER变量来保存选项值。下面两句类似我们一开始在父目录下增加edsionteDriver目录。
 Kconfig文件如下：

```c
# dirvers/edsionteDriver/Kconfig 

menu "Test edsionteDriver" 

config MYDRIVER 
    tristate "mydriver test!" 
config MYDRIVER_USER 
    bool "user-space test" 
    depends on MYDRIVER 

source "drivers/edsionteDriver/key/Kconfig" 
endmenu
```

首先menu和endmenu之间代码创建了一个菜单Test edsionteDriver，然后第一条config语句会建立一个名为”mydriver test”的配置菜单，用户在尽享此项目的配置时输入的配置信息(Y,N或M)，会存储在config后面的MYDRIVER变量中。而这个变量会直接关系到Makefile文件中是否编译相应的文件。第二个config语句还是创建一个配置菜单的条目，只不过比较特殊的是这个菜单依赖于上面我们创建的菜单“mydriver test”（语法上是MYDRIVER_USER依赖于MYDRIVER）。具体含义是，只有当用户选择配置“mydriver test”时，才会出现下级菜单”user-sapce test”；否则这个下级菜单是不会出现的。

最后一条source语句的作用是将edsionteDriver目录下的子目录key/也加入到内核编译时扫描的对应当中。

接下来，就应该修改key目录下的相应文件了，这里不再详细说明，如果你成功修改了上述文件，修改key目录下的文件应该不困难吧？

这样的step by step并不是为了教会各位如何产生那个配置菜单，而是让各位在基本理解Kconfig，Makefile以及.config三者的基础上更深入的去学习。