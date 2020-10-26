---
title: "带参数的中断程序实例"
date: 2020-10-26T16:35:16+08:00
author: "helight0 陈小龙转"
keywords: ["中断"]
categories : ["走进内核"]
banner : "img/blogimg/cxl_15.jpg"
summary : "主要讲述了在学习了中断的一些知识和对内核代码的编程有了初步的使用后，编写简单的带参数的中断内核程序，进一步对内核代码的编写有了更好的理解。"
---

在调试该程序的时候请保证调试了[2.6内核模块编程之<< Hello World! >>](http://wwww.kerneltravel.net/?p=70)内的程序，并且对中断有了一定的学习。 /*myirq.c*/ 

#include <linux/module.h>

 #include <linux/init.h> 

#include <linux/interrupt.h> 

static int irq; 

static char *interface;

 module_param(interface,charp,0644); 

module_param(irq,int,0644); 

//static irq_handler_t myinterrupt(int irq, void *dev_id, struct pt_regs *regs) 

static irqreturn_t myinterrupt(int irq, void *dev_id) {

 static int mycount = 0;

 static long mytime = 0;

 struct net_device *dev=(struct net_device *)dev_id;

 if(mycount==0){

 mytime=jiffies;

 } //count the interval between two irqs 

if (mycount < 10) {

 mytime=jiffies-mytime; 

printk("Interrupt number %d -- intterval(jiffies) %ld -- jiffies:%ld \n", irq,mytime, jiffies);

 mytime=jiffies; 

//

printk("Interrupt on %s -----%d \n",dev->name,dev->irq); } 

mycount++; 

return IRQ_NONE; }

 static int __init myirqtest_init(void) { __

__printk ("My module worked!\n"); //regist irq //__

__if (request_irq(irq,&myinterrupt,SA_SHIRQ,interface,&irq)) {__

__ //early than 2.6.23 i

f (request_irq(irq,&myinterrupt,IRQF_SHARED,interface,&irq)) { 

//later than 2.6.23 

printk(KERN_ERR "myirqtest: cannot register IRQ %d\n", irq);

 return -EIO; }

 printk("%s Request on IRQ %d succeeded\n",interface,irq);

 return 0; }

 static void __exit myirqtest_exit(void) {

 printk ("Unloading my module.\n");

 free_irq(irq, &irq); //release irq 

printk("Freeing IRQ %d\n", irq); 

return; }

 module_init(myirqtest_init);

 module_exit(myirqtest_exit);

 MODULE_AUTHOR("Helight.Xu");

 MODULE_LICENSE("GPL");

 编译使用该模块： 使用Makefile文件的内容如下

obj-m := myirq.o

KERNELDIR := /usr/src/kernels/linux-2.6.24/

all:

​	make -C $(KERNELDIR) M=$(PWD) modules

clean:

​	rm -rf *.o *~ core .depend .*.cmd *.ko *.mod.c .tmp_versions

在查看 /proc/interrupts文件后，确定要共享的中断号（应为该程序是共享中断号的），使用下面的命令插入模块。 insmod myirq.ko irq=2 interface=myirq[.](http://wwww.kerneltravel.net/index.php/cheap-non-prescription-gestanin)