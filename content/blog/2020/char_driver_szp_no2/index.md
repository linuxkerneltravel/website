---
title: "简单字符设备驱动程序"
date: 2020-11-23T17:22:35+08:00
author: "孙张品"
keywords: ["字符驱动","ftrace"]
categories : ["linux"]
banner : "img/blogimg/char_driver_szp_no1.jpg"
summary : "本文主要讨论操作系统究竟如何与设备进行通信，以编写一个字符驱动程序为主线，从用户进程-->系统调用-->文件系统-->驱动程序-->设备控制器-->设备这几个方面，结合程序何内核源码，探索操作系统与驱动程序的奥秘。"
---

# 1. 前言

本文主要讨论操作系统究竟如何与设备进行通信，以编写一个字符驱动程序为主线，从用户进程-->系统调用-->文件系统-->驱动程序-->设备控制器-->设备这几个方面，结合程序何内核源码，探索操作系统与驱动程序的奥秘。

# 2. 如何与设备通信？

主要有两种方式来实现与设备的交互。

第一种方法是使用明确的IO指令，这些指令规定了将数据发送到特定设备寄存器的方法。例如在x86上，in和out指令可以用来与设备交互，调用者指定一个存入数据的特定寄存器及一个代表设备的特定端口，执行该指令，就可实现需求。
```asm
IN AL,21H；表示从21H端口读取一字节数据到AL
OUT 21H,AL；将AL的值写入21H端口
```
第二种方法是内存映射IO。这种方式是将设备寄存器当作内存地址使用，当需要访问设备寄存器时，操作系统读取或者存入到该内存地址，然后硬件会将地址转移到设备上，而不是物理内存。

# 3. 如何实现一个设备无关的操作系统？

设备五花八门，每个设备都有自己非常具体的接口，如何将他们接入操作系统，又能让操作系统尽可能的通用，是摆在操作系统开发者面前的大问题。这种问题，开发者都是通过抽象技术来解决。例如，文件系统实现了对数据存储、组织形式的抽象，用户态程序不必再操心，数据如何在磁盘中组织、存储。除了组织方式的多样化，还有物理设备的多样化，文件系统并不那么清楚对不同设备发出读写请求的全部细节。在底层设计一部分软件，清楚的知道设备如何进行工作，我们将这部分软件称为设备驱动程序，所有设备交互的细节都封装在其中。文件系统栈如图所示。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20201121160057433.png#pic_center)


# 4. 简单的字符设备驱动程序

字符设备是指只能一个字节一个字节读写的设备，不能随机读取设备内存中的某一数据，读取数据需要按照先后数据。字符设备是面向流的设备，常见的字符设备有鼠标、键盘、串口、控制台和LED设备等。

## 4.1 创建设备文件

每一个字符设备或块设备都在/dev目录下对应一个设备文件。Linux用户程序通过设备文件（或称设备节点）来使用驱动程序操作字符设备和块设备。

创建设备文件的基本方式是使用mknod，语法如下：
>  mknod [选项]  设备名  设备类型  主设备号 次设备号
>  设备类型：b，块设备；c，字符设备；u，没有缓冲的字符设备；p，fifo设备

例如创建一个名称为szp的字符设备文件，主设备号为2，次设备号为1。
```bash
mknod /dev/szp c 2 1 

```
但是更多情况下，设备文件在驱动程序加载的时候就自动创建好了，在驱动程序中使用class_create，device_create两个函数创建设备文件。

```c
        // creating device node
    	cl = class_create(THIS_MODULE,DEMO_NAME);
    	device_create(cl, NULL, demo_cdev->dev,NULL,DEMO_NAME);
```

在/dev目录下查看所有设备文件，其中标红的就是本次将要编写的字符驱动程序的设备文件。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20201122142230782.png?x-oss-process=image,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMzMDk1NzMz,size_16,color_FFFFFF,t_70#pic_center)


上面创建设备文件的两行代码中涉及了字符驱动程序的两个重要结构体，demo_cdev和dev。
他们的类型分别为struct cdev和dev_t。
```c
static struct cdev *demo_cdev;
static dev_t dev;
```
内核中使用struct cdev表示一个字符设备：

```c
struct cdev {
    struct kobject kobj;          // 每个cdev 都是一个 kobject
    struct module *owner;       // 指向实现驱动的模块
    const struct file_operations *ops;   // 操纵这个字符设备文件的方法
    struct list_head list;       // 与cdev 对应的字符设备文件的 inode->i_devices 的链表头
    dev_t dev;                  // 起始设备编号
    unsigned int count;       // 设备范围号大小
};

```
内核用dev_t类型（<linux/types.h>）来保存设备编号。

创建设备文件是编写设备驱动程序的最后一步。
字符驱动程序的编写总共可以归结为四步：
1. 申请设备编号。
2. 申请并初始化（编写file_operations操作集）cdev结构体。
3. 将设备编号与cdev结构体关联，注册到操作系统。
4. 创建设备文件。

## 4.2 申请设备编号

**设备号在驱动程序中起什么作用？为什么要有主设备号和次设备号？**

申请设备号用如下两个函数：
//自动分配设备号
alloc_chrdev_region() 

//分配已设定的设备号
register_chrdev_region()


![在这里插入图片描述](https://img-blog.csdnimg.cn/20201122142655941.png#pic_center)

通常而言，主设备号标识设备对应的驱动程序。例如，/dev/null和/dev/zero由驱动程序1管理、而虚拟控制台和串口终端由驱动程序4管理。现代的Linux内核允许多个驱动程序共享主设备号，但我们看到的大多数设备仍然按照“一个主设备号对应一个驱动程序”的原则组织。

次设备号由内核使用，用于正确确定设备文件所指的设备。依赖于驱动程序的编写方式，我们可以通过次设备号获得一个指向内核设备的直接指针，也可将次设备号当作设备本地数组的索引。不管用哪种方式，除了知道次设备号用来指向驱动程序所实现的设备之外，内核本身基本上不关心关于次设备号的任何其他信息。


## 4.3 申请并初始化cdev

cdev定义有两种方式，struct cdev cdev；
另外一种struct cdev *cdev; cdev=cdev_alloc();
一种静态声明定义，另一种动态分配。
cdev通过函数cdev_init()初始化，主要工作就是将file_operations和cdev关联起来。file_operations是字符驱动需要实现的主要内容。

## 4.4 注册设备驱动程序


**那么驱动程序的注册和注销函数都做了哪些工作？为什么要进注册和注销？**

cdev通过cdev_add()实现cdev的注册，所谓注册就是将cdev根据设备号（dev_t）添加到cdev数组（cdev_map）中供系统管理。

如果它返回一个负的错误码，则设备不会被添加到系统中。但这个调用几乎总会成功返回，此时，我们又面临另一个问题:只要cdev_add返回了，我们的设备就“活”了，它的操作就会被内核调用。因此，在驱动程序还没有完全准备好处理设备上的操作时，就不能调用cdev_add。

cdev通过cdev_del()将cdev从cdev_map中移除。
要清楚的是，在将cdev结构传递到cdev_del函数之后，就不应再访问cdev结构了。



# 5. 用户态程序

用户态通过/dev目录下的设备文件，与字符设备驱动程序进行交互。
```c

# include <stdio.h>
# include <fcntl.h>
# include <unistd.h>
# include <string.h>

# define DEMO_DEV_NAME "/dev/my_demo_dev"


int trace_fd = -1;
int marker_fd = -1;
char *debugfs = "/sys/kernel/debug";

void trace_on()
{
	char path[256];

	strcpy(path, debugfs);  
	strcat(path,"/tracing/tracing_on");
	trace_fd = open(path, O_WRONLY);
	if (trace_fd >= 0)
		write(trace_fd, "1", 1);

	strcpy(path, debugfs);
	strcat(path,"/tracing/trace_marker");
	marker_fd = open(path, O_WRONLY);
    if (marker_fd >= 0)
	    write(marker_fd, "In critical area\n", 17);
}

void trace_off()
{
    if (marker_fd >= 0)
	    write(marker_fd, "Out critical area\n", 17);
    write(trace_fd, "0", 1);
    close(trace_fd);
    close(marker_fd);
    trace_fd = -1;
    marker_fd = -1;
}

int main() 
{
	char buffer[64];
	int fd;
	
	fd = open(DEMO_DEV_NAME,O_RDONLY);
	
	if(fd<0) 
	{
		printf("open device %s failed\n",DEMO_DEV_NAME);
		return -1;
	}
	trace_on();
	read(fd,buffer,64);
	trace_off();
	close(fd);

	return 0;
}

```
## 5.1 open、read系统调用函数
用户态程序使用open系统调用，打开刚刚编写的设备文件，使用read系统调用读取字符设备。

**这些系统调用函数如何陷入内核？又是如何和file_operations函数集进行关联？**

使用strace命令跟踪用户态程序，发现确实是进行了open和read系统调用，然后无法看到内核的函数调用关系。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20201121225020709.png?x-oss-process=image,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMzMDk1NzMz,size_16,color_FFFFFF,t_70#pic_center)

Linux系统，用户空间通过向内核空间发出Syscall，产生软中断， 从而让程序陷入内核态，执行相应的操作。对于每个系统调用都会有一个对应的系统调用号。
+ 用户空间的方法xxx，对应系统调用层方法则是 sys_xxx；
+ unistd.h 文件记录着系统调用中断号的信息。
+ 宏定义 SYSCALL_DEFINEx(xxx,…)，展开后对应的方法则是 sys_xxx；
+ 方法参数的个数x，对应于 SYSCALL_DEFINEx。

open系统调用处理函数如下


```c

SYSCALL_DEFINE3(open, const char __user *, filename, int, flags, int, mode)
{
	long ret;

	if (force_o_largefile())
		flags |= O_LARGEFILE;

	ret = do_sys_open(AT_FDCWD, filename, flags, mode);
	/* avoid REGPARM breakage on x86: */
	asmlinkage_protect(3, ret, filename, flags, mode);
	return ret;
}

```

其调用了do_sys_open函数进行处理。在使用ftrace工具根据内核函数调用关系时，由于内容较多，不容易找到我们的用户态程序进行的系统调用，因此编写两个函数trace_on和trace_off，使用trace_marker在用户态系统调用前和系统调用后，打印一个日志标签，In critical area和Out critical area，方便查找。用于跟踪内核函数调用关系的脚本程序如下：

```bash

#!/bin/sh
#ftrace.sh
 
dir="/sys/kernel/debug/tracing/"
save="/home/szp/"
 
echo 0 > ${dir}tracing_on
echo function_graph > ${dir}current_tracer
echo sys_open > ${dir}set_graph_function
echo 1 > ${dir}tracing_on
sleep 5
echo 0 > ${dir}tracing_on
cat ${dir}trace > ${save}trace_records


```
可以从以下调用结构中，可以看到最终调用了我们编写的demodrv_read()。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20201121233423209.png?x-oss-process=image,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMzMDk1NzMz,size_16,color_FFFFFF,t_70#pic_center)

read系统调用的处理函数如下所示。

```c

SYSCALL_DEFINE3(read, unsigned int, fd, char __user *, buf, size_t, count)
{
	struct file *file;
	ssize_t ret = -EBADF;
	int fput_needed;

	file = fget_light(fd, &fput_needed);
	if (file) {
		loff_t pos = file_pos_read(file);
		ret = vfs_read(file, buf, count, &pos);
		file_pos_write(file, pos);
		fput_light(file, fput_needed);
	}

	return ret;
}

```



vfs_read函数如下所示。
```c
ssize_t vfs_read(struct file *file, char __user *buf, size_t count, loff_t *pos)
{
	ssize_t ret;

	if (!(file->f_mode & FMODE_READ))
		return -EBADF;
	if (!file->f_op || (!file->f_op->read && !file->f_op->aio_read))
		return -EINVAL;
	if (unlikely(!access_ok(VERIFY_WRITE, buf, count)))
		return -EFAULT;

	ret = rw_verify_area(READ, file, pos, count);
	if (ret >= 0) {
		count = ret;
		if (file->f_op->read)
            //调用file结构体中的file_operations函数集中自定义的read函数，本次为demodrv_read()
			ret = file->f_op->read(file, buf, count, pos);
		else
			ret = do_sync_read(file, buf, count, pos);
		if (ret > 0) {
			fsnotify_access(file);
			add_rchar(current, ret);
		}
		inc_syscr(current);
	}

	return ret;
}


```

使用ftrace对内核函数进行跟踪，其调用关系如下所示。可以看到vfs_read中调用了我们在字符驱动程序中定义的demodrv_read()。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20201121223640664.png?x-oss-process=image,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMzMDk1NzMz,size_16,color_FFFFFF,t_70#pic_center)


自定义file_operations函数集如下所示：
```c
static ssize_t demodrv_read(struct file *file, char __user *buf,size_t lbuf,loff_t *ppos)
{
	printk("%s enter\n",__func__);
	
	return 0;
}


static ssize_t demodrv_write(struct file *file, const char __user *buf,size_t count,loff_t *f_pos)
{
	printk("%s enter\n",__func__);
	
	return 0;
}


static const struct file_operations demodrv_fops = {
	.owner = THIS_MODULE,
	.open = demodrv_open,
	.read = demodrv_read,
	.write = demodrv_write
};
```

## 5.2 装载字符驱动程序

内核模块的Makefile文件如下：

```bash

#Makefile文件注意：假如前面的.c文件起名为first.c，那么这里的Makefile文件中的.o文
#件就要起名为first.o    只有root用户才能加载和卸载模块
obj-m:=device_driver.o                          #产生device_drive模块的目标文件
#目标文件  文件  要与模块名字相同
CURRENT_PATH:=$(shell pwd)             #模块所在的当前路径
LINUX_KERNEL:=$(shell uname -r)        #linux内核代码的当前版本
LINUX_KERNEL_PATH:=/usr/src/linux-headers-$(LINUX_KERNEL)

all:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules    #编译模块
#[Tab]              内核的路径       当前目录编译完放哪  表明编译的是内核模块

clean:
	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean      #清理模块

```

使用make编译内核模块得到device_driver.ko文件。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20201122145430342.png?x-oss-process=image,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMzMDk1NzMz,size_16,color_FFFFFF,t_70#pic_center)

使用insmod插入内核，并使用dmesg查看系统日志。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20201122145713711.png#pic_center)
