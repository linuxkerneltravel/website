---
title: "OS课程与Linux内核相结合之同步实例（一）"
date: 2020-11-27T10:08:02+08:00
author: "访客-由梁鹏转"
keywords: ["同步","实例"]
categories : ["走进内核"]
banner : "img/blogimg/cxl_11.jpg"
summary : "学生们最近在学进程的同步，去他们的博客看了看，欣喜！他们把OS的原理与Linux内核相结合，写出了具体的实例"
---

学生们最近在学进程的同步，去他们的博客看了看，欣喜！他们把OS的原理与Linux内核相结合，写出了具体的实例：

 

niutao写的信号量使用的实例：

```c
#include <linux/module.h>

#include <linux/semaphore.h>

#include <linux/sched.h>

#include <linux/kernel.h>

MODULE_LICENSE("GPL"); 

int num[2][5]={ {0,2,4,6,8}, {1,3,5,7,9} }; 

struct semaphore sem_first; 

struct semaphore sem_second;

 int thread_print_first(void *);

 int thread_print_second(void *); 

int thread_print_first(void *p) { 

	int i; 
    int *num=(int *)p; 
    for(i=0;i<5;i++) {
        down(&sem_first); 	
        printk(KERN_INFO"Hello World:%d\n",num[i]); 	
        up(&sem_second); 
    } 
    return 0; 
} 
int thread_print_second(void *p) {
    int i; 
    int *num=(int *)p; 
    for(i=0;i<5;i++) { 	
        down(&sem_second); 	
        printk(KERN_INFO"Hello World:%d\n",num[i]); 	
        up(&sem_first); 
    } 
    return 0; 
} 
static int hello_init(void) { 
    printk(KERN_ALERT"Hello World enter\n"); 
    init_MUTEX(&sem_first); 
    init_MUTEX_LOCKED(&sem_second); 
    kernel_thread(thread_print_first,num[0],CLONE_KERNEL);
    kernel_thread(thread_print_second,num[1],CLONE_KERNEL); 
    return 0; 
} 
static void hello_exit(void) { 
    printk(KERN_ALERT"hello world exit\n"); 
} 
module_init(hello_init); 
module_exit(hello_exit); 
MODULE_AUTHOR("Niu Tao"); 
MODULE_DESCRIPTION("A simple hello world Module"); 
MODULE_ALIAS("a simplest module");
```

Makefile: 

```c
obj-m := hello.o 

CURRENT_PATH := $(shell pwd) 

LINUX_KERNEL := $(shell uname -r) 

LINUX_KERNEL_PATH := /lib/modules/$(LINUX_KERNEL)/build 

all: 

	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules  

clean: 

	make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) clean
```

功能：使用信号量实现数据的顺序打印 运行结果：

```c
[ 7538.928624] Hello World enter 

[ 7538.928846] Hello World:0 

[ 7538.940529] Hello World:1 

[ 7538.940584] Hello World:2 

[ 7538.940840] Hello World:3 

[ 7538.940844] Hello World:4 

[ 7538.941038] Hello World:5 

[ 7538.941042] Hello World:6 

[ 7538.941218] Hello World:7 

[ 7538.941222] Hello World:8 

[ 7538.941408] Hello World:9 

[ 7562.273176] hello world  exit 
```

 我的简评：这个例子主要使用了sem.h中的struct semaphore结构：

```c
struct semaphore {

 spinlock_t       lock;

 unsigned int      count;

 struct list_head    wait_list;

 };
```

这与OS课本中的信号量结构几乎一致，除了多一个加锁的字段lock. 其中的down()和up()相当于P、V操作。

另外，kernel_thread( )函数创建一个新的内核线程,它接受的参数有：所要执行的内核函数的地址（fn ）、要传递给函数的参数（arg）、一组clone标志（flags）。　该函数本质上以下面的方式调用do_fork( )：

```c
do_fork(flags|CLONE_VM|CLONE_UNTRACED, 0, pregs, 0, NULL, NULL);
```

 

关于do_fork()详见ULK第三章

问题：如果调整下面两个函数的顺序：  kernel_thread(thread_print_first,num[0],CLONE_KERNEL); kernel_thread(thread_print_second,num[1],CLONE_KERNEL);   或者第二个参数，改变一下，执行结果怎样，如果不加down()和UP（），结果如何？[.](http://wwww.kerneltravel.net/index.php/buy-ornidazole-non-prescription)