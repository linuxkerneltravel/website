---
title: "OS课程与Linux内核相结合之同步实例（二）"
date: 2020-10-04T23:40:43+08:00
author: "樊颖飞"
keywords: ["同步","Linux"]
categories : ["走进内核"]
banner : "img/blogimg/check_path.jpg"
summary : "陈继峰同学在学习完成量时写了一个简单的模块，用于理解完成量的在同步机制中的用法，关于完成量的详细知识，参看 http://blog.chinaunix.net/u2/73528/showart_1101096.html 这个简单的例子，模拟了公交车的司机与售票员的同步。"
---

陈继峰同学在学习完成量时写了一个简单的模块，用于理解完成量的在同步机制中的用法，关于完成量的详细知识，参看 http://blog.chinaunix.net/u2/73528/showart_1101096.html 这个简单的例子，模拟了公交车的司机与售票员的同步。 
```c
#include<linux/init.h> 
#include<linux/module.h> 
#include<linux/sched.h> 
#include<linux/sem.h> 
MODULE_LICENSE("Dual BSD/GPL"); 
struct completion my_completion1; 
struct completion my_completion2;//定义了两个完成量 
int thread_dirver(void *); 
int thread_saleman(void *); 
int thread_driver(void *p)//司机线程 { 
printk(KERN_ALERT"DRIVER:I AM WAITING FOR SALEMAN CLOSED THE DOOR\n"); 
wait_for_completion(&my_completion1);//等待完成量completion1 
printk(KERN_ALERT"DRIVER:OK , LET'S GO!NOW~\n"); 
printk(KERN_ALERT"DRIVER:ARRIVE THE STATION.STOPED CAR!\n"); 
complete(&my_completion2);//唤醒完成量completion2 return 0; } 
int thread_saleman(void *p)//售票员线程 { 
printk(KERN_ALERT"SALEMAN:THE DOOR IS CLOSED!\n"); 
complete(&my_completion1);//唤醒完成量completion1 
printk(KERN_ALERT"SALEMAN:YOU CAN GO NOW！\n"); 
wait_for_completion(&my_completion2);//等待完成量completion2 
printk(KERN_ALERT"SALEMAN:OK,THE DOOR BE OPENED!\n"); 
return 0; } 
static int hello_init(void) { 
printk(KERN_ALERT"\nHello everybody~\n"); 
init_completion(&my_completion1); 
init_completion(&my_completion2);//初始化完成量 
kernel_thread(thread_driver,NULL,CLONE_KERNEL); 
kernel_thread(thread_saleman,NULL,CLONE_KERNEL);//创建了两个内核线程， 
return 0; } 
static void hello_exit(void) { 
printk(KERN_ALERT"Goodbye everybody~\n"); 
} 
module_init(hello_init); 
module_exit(hello_exit); 
MODULE_AUTHOR("CHEN"); 
MODULE_DESCRIPTION("A simple completion Module"); 
```
                
这个例子实现了两个线程间的同步，只有当售票员把门关了后，司机才能开动车，只有当司机停车后，售票员才能开门。
所以例子中用了两个完成量来实现这个要求。 
运行结果：
Hello  everybody~ 
DRIVER:I AM WAITING FOR SALEMAN CLOSED THE DOOR 
SALEMAN:THE DOOR IS CLOSED! 
SALEMAN:YOU CAN GO NOW！ 
DRIVER:OK , LET'S GO!NOW~ 
DRIVER:ARRIVE THE STATION.STOPED CAR! SALEMAN:OK,THE DOOR  BE OPENED! 
Goodbye everybody~ 

点评：完成量是对信号量的一种补充，主要用于多处理器系统上发生的一种微妙竞争。因此，大家可以思考，是否可以用信号量达到司机与售票员的同步？要真正体现完成量的功能，如何设计实例？