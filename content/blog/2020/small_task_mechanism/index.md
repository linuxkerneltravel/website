---
title: "小任务机制实例"
date: 2008-11-10T16:50:24+08:00
author: "helight0"
keywords: ["中断"]
categories : ["新手上路"]
banner : "img/blogimg/3.png"
summary : "在调试该程序的时候请保证调试了带参数的中断程序实例内的程序，并且对中断有了一定的学习。"
---

在调试该程序的时候请保证调试了[带参数的中断程序实例](http://wwww.kerneltravel.net/?p=133)内的程序，并且对中断有了一定的学习。 

```c
#include <linux/module.h> 
#include <linux/init.h> 
#include <linux/kernel.h> 
#include <linux/interrupt.h> 

static int irq; 
static char *interface;

module_param(interface,charp,0644); 
module_param(irq,int,0644); 

static int mycount = 0; 
static long mytime = 0; 
static unsigned long data=0;
static struct tasklet_struct mytasklet;//定义小任务 

//小任务函数 
static void mylet(unsigned long data) { 
    printk("tasklet running.\n"); 
    if(mycount==0)
        mytime=jiffies; 
    if (mycount < 10){ 
        mytime=jiffies-mytime;
        printk("Interrupt number %d --time %ld \n",irq,mytime);
        mytime=jiffies; 
    } 
    mycount++; 
    return; 
} 

//中断服务程序 
static irqreturn_t myinterrupt(int intno,void *dev_id) {
    tasklet_schedule(&mytasklet);//调度小任务，让它运行
    return IRQ_NONE;
} 

static int __init mytasklet_init(void) { 
    printk("init...\n"); 
    tasklet_init(&mytasklet, mylet,data);//初始化小任务 
    tasklet_schedule(&mytasklet); 
    if (request_irq(irq,&myinterrupt,IRQF_SHARED,interface,&irq)) { 
        printk(KERN_ERR "myirqtest: cannot register IRQ %d\n", irq); 
        tasklet_kill(&mytasklet);//删除小任务 free_irq(irq,&irq);//释放中断 
        return -EIO;
    } 
    printk("%s Request on IRQ %d succeeded\n",interface,irq);
    return 0;
} 

static void __exit mytasklet_exit(void) { 
    tasklet_kill(&mytasklet);//删除小任务 free_irq(irq,&irq);//释放中断 
    printk("Freeing IRQ %d\n", irq); printk("exit...\n");
    return; 
} 

MODULE_AUTHOR("Helight.Xu");
MODULE_LICENSE("GPL"); 
module_init(mytasklet_init); 
module_exit(mytasklet_exit);
```

