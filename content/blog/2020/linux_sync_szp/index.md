---
title: "OS课程与Linux内核相结合之同步实例（三）"
date: 2020-10-04T23:32:18+08:00
author: "樊颖飞"
keywords: ["同步","Linux"]
categories : ["走进内核"]
banner : "img/blogimg/linuxer.png"
summary : "在操作系统中讲到进程同步的问题的时候，都会讲一些经典的例子，其中最经典的当属“生产者和消费者的问题”。"
---

在操作系统中讲到进程同步的问题的时候，都会讲一些经典的例子，其中最经典的当属“生产者和消费者的问题”。

生产者和消费者的规则是生产者生产一个产品后，消费者才能消费，并且在消费者还没有消费已经生产的产品的时候，生产者是不能再进行生产的。

牛涛写的这个例子演示了这一过程： 
```c
#include<linux/init.h> 
#include<linux/module.h> 
#include<linux/sem.h> 
#include<linux/sched.h> 
MODULE_LICENSE("Dual BSD/GPL"); 
struct semaphore sem_producer;/*"生产需求证",在产品没有被消费的时候不能再进行生产*/ 
struct semaphore sem_consumer;/*"消费证"，在有产品的时候(可以获得该锁)才可以消费*/ 
char product[10];/*"产品"存放地*/ 
int exit_flags;/*生产线开启标志*/ 
int producer(void *product);/*生产者*/ 
int consumer(void *product);/*消费者*/ 
static int procon_init(void) { 
printk(KERN_INFO"show producer and consumer\n"); 
init_MUTEX(&sem_producer);/*购买"生产需求证"，并且准许生产*/ init_MUTEX_LOCKED(&sem_consumer);/*购买"消费证"，但不可消费*/ exit_flags=0;/*生产线可以开工*/ 
kernel_thread(producer,product,CLONE_KERNEL);/*启动生产者*/ kernel_thread(consumer,product,CLONE_KERNEL);/*启动消费者*/ 
return 0; 
} 
static void procon_exit(void) { 
    printk(KERN_INFO"exit producer and consumer\n"); } /* * 生产者，负责生产十个产品 */ 
    int producer(void *p) { 
        char *product=(char *)p; 
        int i; for(i=0;i<10;i++) { /*总共生产十个产品*/ /* 查看"生产需求证"，如果产品已经被消费， * 则准许生产。否则在此等待直到需要生产 */ 
        down(&sem_producer); 
        snprintf(product,10,"product-%d",i);/*生产产品*/ 
        printk(KERN_INFO"producer produce %s\n",product);/*生产者提示已经生产*/ 
        up(&sem_consumer);/*为消费者发放"消费证"*/ } 
        exit_flags=1;/*生产完毕，关闭生产线*/ 
        return 0; } /* * 消费者，如果有产品，则消费产品 */ 
        int consumer(void *p) { 
            char *product=(char *)p; 
            for(;;) { 
            if(exit_flags) /*如果生产工厂已经关闭，则停止消费*/ break; /*获取"消费证"，如果有产品，则可以获取， *进行消费。否则等待直到有产品。 */ 
            down(&sem_consumer); 
            printk(KERN_INFO"consumer consume %s\n",product);/*消费者提示获得了产品*/ 
            memset(product,'\0',10);/*消费产品*/ 
            up(&sem_producer);/*向生产者提出生产需求*/ 
            } 
            return 0; } 
            module_init(procon_init); 
            module_exit(procon_exit); 
            MODULE_AUTHOR("Niu Tao"); 
            MODULE_DESCRIPTION("producer and consumer Module"); 
            MODULE_ALIAS("a simplest module"); 
``` 
                
                
Makefile: 

obj-m :=procon.o 
CURRENT_PATH := $(shell pwd) 
LINUX_KERNEL := $(shell uname -r) 
LINUX_KERNEL_PATH := /usr/src/linux-headers-$(LINUX_KERNEL) all: make -C $(LINUX_KERNEL_PATH) M=$(CURRENT_PATH) modules 

clean: rm -rf .*.cmd *.o *.mod.c *.ko .tmp_versions Module.symvers .Makefile.swp 

运行结果： 
[ 5684.818139] show producer and consumer 
[ 5684.818352] producer produce product-0 
[ 5684.819746] consumer consume product-0 
[ 5684.819811] producer produce product-1 
[ 5684.820061] consumer consume product-1 
[ 5684.820065] producer produce product-2 
[ 5684.820267] consumer consume product-2 
[ 5684.820271] producer produce product-3 
[ 5684.820498] consumer consume product-3 
[ 5684.820503] producer produce product-4 
[ 5684.820822] consumer consume product-4 
[ 5684.820828] producer produce product-5 
[ 5684.821062] consumer consume product-5 
[ 5684.821067] producer produce product-6 
[ 5684.821274] consumer consume product-6 
[ 5684.821279] producer produce product-7 
[ 5684.821475] consumer consume product-7 
[ 5684.821479] producer produce product-8 
[ 5684.821681] consumer consume product-8 
[ 5684.821685] producer produce product-9 
[ 5684.821897] consumer consume product-9 
[ 5705.169930] exit producer and consumer 

点评： 这个例子很好的演绎了生产者和消费者互相合作的过程。 不过，从输出结果看，是生产一个产品就马上消费一个，与实际情况不太符合，能否够修改程序，让生产和消费无序进行？