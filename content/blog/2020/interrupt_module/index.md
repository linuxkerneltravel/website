---
title: "内核模块编程之进阶（四）-编写带参数的中断模块"
date: 2008-11-09T16:50:24+08:00
author: "helight0"
keywords: ["内核模块"]
categories : ["内核模块"]
banner : "img/blogimg/1.png"
summary : "在此，我们将编写一个模块，其中有一个中断函数，当内核接收到某个 IRQ 上的一个中断时会调用它。先给出全部代码，读者自己调试，把对该程序的理解跟到本贴后面。"
---

```c
#include <linux/module.h>
#include <linux/init.h> 
#include <linux/interrupt.h>

static int irq; 
static char *interface; 
//MODULE_PARM_DESC(interface,"A network interface"); 2.4内核中该宏的用法 
//molule_parm(interface,charp,0644) ;
//2.6内核中的宏 
//MODULE_PARM_DESC(irq,"The IRQ of the network interface"); 
module_param(irq,int,0644); 
static irqreturn_t myinterrupt(int irq, void *dev_id, struct pt_regs *regs) { 
    static int mycount = 0;
    if (mycount < 10) { 
        printk("Interrupt!\n");
        mycount++; 
    } 
    return IRQ_NONE; 
} 
static int __init myirqtest_init(void) { 
    printk ("My module worked!11111\n"); 
    if (request_irq(irq, &myinterrupt, SA_SHIRQ,interface, &irq)) { 
        printk(KERN_ERR "myirqtest: cannot register IRQ %d\n", irq);
        return -EIO;
    } 
    printk("%s Request on IRQ %d succeeded\n",interface,irq); 
    return 0;
} 
static void __exit myirqtest_exit(void) { 
    printk ("Unloading my module.\n"); 
    free_irq(irq, &irq); 
    printk("Freeing IRQ %d\n", irq);
    return; 
} 

module_init(myirqtest_init); 
module_exit(myirqtest_exit); 
MODULE_LICENSE("GPL"); 
```

​		这里要说明的是，在插入模块时，可以带两个参数，例如 insmod myirq.ko interface=eth0 irq=9 其中 具体网卡 irq的值可以查看 cat /proc/interrupts 。

​		动手吧！以此为例，可以设计出各种各样有价值的内核模块，贴出来体验分享的快乐吧。[.](http://wwww.kerneltravel.net/index.php/allegra-da-100)