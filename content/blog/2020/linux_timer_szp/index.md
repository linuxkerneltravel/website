---
title: "Linux定时器使用"
date: 2020-10-04T23:21:16+08:00
author: "樊颖飞"
keywords: ["Timer","Linux"]
categories : ["新手上路"]
banner : "img/blogimg/io_sys_szp.jpg"
summary : "Linux定时器使用，定时器在内核的定义，使用定时器的步骤，运行结果。"
---

定时器在内核的定义： 
```c
12 struct timer_list { 
    13 /* 
    14 * All fields that change during normal runtime grouped to the 
    15 * same cacheline 16 */ 
    17 struct list_head entry; //定时器的链表 
    18 unsigned long expires;//以节拍为单位的定时时间，表示为定时器触发的到期时间 
    19 struct tvec_base *base; 
    20 
    21 void (*function)(unsigned long); //该指针指向定时器处理函数，函数参数为长整形 
    22 unsigned long data; //处理函数的参数值 
    23 
    24 int slack; 
    25 
    26 #ifdef CONFIG_TIMER_STATS 
    27 void *start_site; 
    28 char start_comm[16]; 
    29 int start_pid; 
    30 #endif 
    31 #ifdef CONFIG_LOCKDEP 
    32 struct lockdep_map lockdep_map; 
    33 #endif 
    34 }; 

```

使用定时器的步骤： 
1） 定义定时器: struct timer_list my_timer 

2）初始化定时器： 初始化定时器的到期节拍数 my_timer.expires = jiffies +delay ;该设置让定时器的触发时间设置为 激活定时器后的delay个节拍点 my_timer.function = 处理函数的名称 该设置设置定时器触发时处理的函数 my_timer.data 初始化处理函数的参数值，若处理函数没有参数则可简单设置为0或任意其他数值 

3）激活定时器：即内核会开始定时，直到my_timer.expires 使用函数add_timer 即 add_timer(&my_timer); 内核原型为： 

```c
    849 void add_timer(struct timer_list *timer) 
    850 { 
    851 BUG_ON(timer_pending(timer)); 
    852 mod_timer(timer, timer->expires); //该函数设置定时器timer的定时时间为timer->expires; 
    
    853 } 
```
    
4)删除定时器：如果需要在定时器到期之前停止定时器，则可以使用该函数，若是定时器已经过期则不需调用该函数，因为它们会自动删除 del_timer(&my_timer); 

定时器的简单实例：该例子的功能是首先初始化一个定时器，当定时器时间到后触发定时器出俩函数的执行，该函数又重新设置了该定时器的时间，即该定时器又在下一次定时时间的到来继续处理函数，一直循环，知道最后在该模块卸载时进行删除定时器，结束该定时器代码中 HZ为内核每一秒的节拍数，是通过宏进行定义的，通过该程序的打印结果可以得到，
本人电脑的节拍数测试结果为
```c
250 #include< linux/module.h > 
#include< linux/init.h > 
#include< linux/sched.h > 
#include < linux/timer.h > 
#include < linux/kernel.h > 
struct timer_list stimer; //定义定时器 
static void time_handler(unsigned long data){ //定时器处理函数 mod_timer(&stimer, jiffies + HZ); 
printk("current jiffies is %ld\n", jiffies); } 
static int __init timer_init(void){ //定时器初始化过程 
printk("My module worked!\n"); 
init_timer(&stimer); 
stimer.data = 0; 
stimer.expires = jiffies + HZ; //设置到期时间 
stimer.function = time_handler; add_timer(&stimer); 
return 0; } 
static void __exit timer_exit(void){ 
printk("Unloading my module.\n"); 
del_timer(&stimer);//删除定时器 return; } 

module_init(timer_init);//加载模块 
module_exit(timer_exit);//卸载模块 
MODULE_AUTHOR("fyf"); 
MODULE_LICENSE("GPL"); 
```
加载/ 卸载该程序后通过命令dmesg可以看到 

[ 6225.522208] My module worked! 
[ 6226.520014] current jiffies is 1481630 
[ 6227.520014] current jiffies is 1481880 
[ 6228.520013] current jiffies is 1482130 
[ 6229.520011] current jiffies is 1482380 
[ 6229.770335] Unloading my module. 

即每2次的jiffies之差为250 定时器的应用：以下是一个简单的延迟当前进程执行的程序，延迟是通过定时器来实现的； 
```c
#include< linux/module.h > 
#include< linux/init.h > 
#include< linux/sched.h > 
#include < linux/timer.h > 
#include < linux/kernel.h > 
struct timer_list stimer; //定义定时器 
int timeout = 10 * HZ; 
static void time_handler(unsigned long data){ //定时器处理函数，执行该函数获取挂起进程的pid，唤醒该进程 
struct task_struct *p = (struct task_struct *)data;//参数为挂起进程pid wake_up_process(p);//唤醒进程 

printk("current jiffies is %ld\n", jiffies); //打印当前jiffies } static int __init timer_init(void){ //定时器初始化过程 
printk("My module worked!\n"); 
init_timer(&stimer); stimer.data = (unsigned long)current; //将当前进程的pid作为参数传递 
stimer.expires = jiffies + timeout; //设置到期时间 
stimer.function = time_handler; 
add_timer(&stimer); 
printk("current jiffies is %ld\n", jiffies); 
set_current_state(TASK_INTERRUPTIBLE); 
schedule(); //挂起该进程 
del_timer(&stimer); //删除定时器 
return 0; } 
static void __exit timer_exit(void){ 
printk("Unloading my module.\n"); 
return; } 
module_init(timer_init);//加载模块 
module_exit(timer_exit);//卸载模块 
MODULE_AUTHOR("fyf"); 
MODULE_LICENSE("GPL"); 
```

运行结果： 
[ 9850.099121] My module worked! 
[ 9850.099127] current jiffies is 2387524 
[ 9860.128017] current jiffies is 2390032 
[ 9869.135805] Unloading my module. 

打印结果与定时时间2500有一点差距，是因为打印时第一次的jiffies实在add_timer之后打印的，故不是定时器激发时的jiffies，第二次同理，所以结果不是确定的，但都于2500相差不多.