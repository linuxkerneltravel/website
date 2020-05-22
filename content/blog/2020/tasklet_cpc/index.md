---
title: "中断处理的tasklet（小任务）机制"
date: 2020-05-23T04:20:45+08:00
author: "admin001"
keywords: ["中断处理","tasklet"]
categories : ["LINUX内核试验"]
banner : "img/blogimg/Network.jpg"
summary : "中断服务程序一般都是在中断请求关闭的条件下执行的,以避免嵌套而使中断控制复杂化。下面介绍中断处理的tasklet机制"
---
中断服务程序一般都是在中断请求关闭的条件下执行的,以避免嵌套而使中断控制复杂化。但是，中断是一个随机事件，它随时会到来，如果关中断的时间太长，CPU就不能及时响应其他的中断请求，从而造成中断的丢失。因此，内核的目标就是尽可能快的处理完中断请求，尽其所能把更多的处理向后推迟。例如，假设一个数据块已经达到了网线，当中断控制器接受到这个中断请求信号时，Linux内核只是简单地标志数据到来了，然后让处理器恢复到它以前运行的状态，其余的处理稍后再进行（如把数据移入一个缓冲区，接受数据的进程就可以在缓冲区找到数据）。因此，内核把中断处理分为两部分：上半部（top half）和下半部（bottom half），上半部（就是中断服务程序）内核立即执行，而下半部（就是一些内核函数）留着稍后处理：

 

首先，一个快速的“上半部”来处理硬件发出的请求，它必须在一个新的中断产生之前终止。通常，除了在设备和一些内存缓冲区（如果你的设备用到了DMA，就不止这些）之间移动或传送数据，确定硬件是否处于健全的状态之外，这一部分做的工作很少。

下半部运行时是允许中断请求的，而上半部运行时是关中断的，这是二者之间的主要区别。                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                 

但是，内核到底什时候执行下半部，以何种方式组织下半部？这就是我们要讨论的下半部实现机制，这种机制在内核的演变过程中不断得到改进，在以前的内核中，这个机制叫做bottom half(简称bh)，在2.4以后的版本中有了新的发展和改进，改进的目标使下半部可以在多处理机上并行执行，并有助于驱动程序的开发者进行驱动程序的开发。下面主要介绍常用的小任务(Tasklet)机制及2.6内核中的工作队列机制。除此之外，还简要介绍2.4以前内核中的下半部和任务队列机制。

## 1 小任务机制 
这里的小任务是指**对要推迟执行的函数进行组织的一种机制**。其数据结构为tasklet_struct，每个结构代表一个独立的小任务，其定义如下：

```c
struct tasklet_struct {

struct tasklet_struct *next;        /*指向链表中的下一个结构*/

     unsigned long state;            /* 小任务的状态 */

     atomic_t count;    /* 引用计数器 */

     void (*func) (unsigned long);            /* 要调用的函数 */

     unsigned long data;           /* 传递给函数的参数 */

}；
```
结构中的func域就是下半部中要推迟执行的函数 ，data是它唯一的参数。

State域的取值为TASKLET_STATE_SCHED或TASKLET_STATE_RUN。TASKLET_STATE_SCHED表示小任务已被调度，正准备投入运行，TASKLET_STATE_RUN表示小任务正在运行。TASKLET_STATE_RUN只有在多处理器系统上才使用，单处理器系统什么时候都清楚一个小任务是不是正在运行（它要么就是当前正在执行的代码，要么不是）。

Count域是小任务的引用计数器。如果它不为0，则小任务被禁止，不允许执行；只有当它为零，小任务才被激活，并且在被设置为挂起时，小任务才能够执行。
## 2 声明和使用小任务
大多数情况下，为了控制一个寻常的硬件设备，小任务机制是实现下半部的最佳选择。小任务可以动态创建，使用方便，执行起来也比较快。

我们既可以静态地创建小任务，也可以动态地创建它。选择那种方式取决于到底是想要对小任务进行直接引用还是一个间接引用。如果准备静态地创建一个小任务（也就是对它直接引用），使用下面两个宏中的一个：

DECLARE_TASKLET(name, func, data)

DECLARE_TASKLET_DISABLED(name, func, data)

这两个宏都能根据给定的名字静态地创建一个tasklet_struct结构。当该小任务被调度以后，给定的函数func会被执行，它的参数由data给出。这两个宏之间的区别在于引用计数器的初始值设置不同。第一个宏把创建的小任务的引用计数器设置为0，因此，该小任务处于激活状态。另一个把引用计数器设置为1，所以该小任务处于禁止状态。例如：

DECLARE_TASKLET(my_tasklet, my_tasklet_handler, dev);

这行代码其实等价于

struct tasklet_struct my_tasklet = { NULL, 0, ATOMIC_INIT(0), tasklet_handler, dev};

这样就创建了一个名为my_tasklet的小任务，其处理程序为tasklet_handler，并且已被激活。当处理程序被调用的时候，dev就会被传递给它。
## 3 编写自己的小任务处理程序
小任务处理程序必须符合如下的函数类型：

void tasklet_handler(unsigned long data)

由于小任务不能睡眠，因此不能在小任务中使用信号量或者其它产生阻塞的函数。但是小任务运行时可以响应中断。
## 4 调度自己的小任务
通过调用tasklet_schedule()函数并传递给它相应的tasklt_struct指针，该小任务就会被调度以便适当的时候执行：

tasklet_schedule(&my_tasklet);  /*把 my_tasklet 标记为挂起 */

在小任务被调度以后，只要有机会它就会尽可能早的运行。在它还没有得到运行机会之前，如果一个相同的小任务又被调度了，那么它仍然只会运行一次。
可以调用tasklet_disable()函数来禁止某个指定的小任务。如果该小任务当前正在执行，这个函数会等到它执行完毕再返回。调用tasklet_enable()函数可以激活一个小任务，如果希望把以DECLARE_TASKLET_DISABLED（）创建的小任务激活，也得调用这个函数，如：

tasklet_disable(&my_tasklet);     /* 小任务现在被禁止,这个小任务不能运行 */

tasklet_enable(&my_tasklet);    /*  小任务现在被激活 */

 

也可以调用tasklet_kill()函数从挂起的队列中去掉一个小任务。该函数的参数是一个指向某个小任务的tasklet_struct的长指针。在小任务重新调度它自身的时候，从挂起的队列中移去已调度的小任务会很有用。这个函数首先等待该小任务执行完毕，然后再将它移去。
## 5  tasklet的简单用法
下面是tasklet的一个简单应用, 以模块的形成加载。

```c
#include <linux/module.h> 
#include <linux/init.h> 
#include <linux/fs.h> 
#include <linux/kdev_t.h> 
#include <linux/cdev.h> 
#include <linux/kernel.h> 
#include <linux/interrupt.h>   
static struct tasklet_struct my_tasklet;   
static void tasklet_handler (unsigned long data) {  
	printk(KERN_ALERT "tasklet_handler is running.\n"); }   	
static int __init test_init(void) {         
	tasklet_init(&my_tasklet, tasklet_handler, 0);  
	tasklet_schedule(&my_tasklet);         
	return 0; }   
static void __exit test_exit(void) {         
	tasklet_kill(&my_tasklet);         
	printk(KERN_ALERT "test_exit running.\n"); } 
MODULE_LICENSE("GPL");   
module_init (test_init);
module_exit(test_exit); 
```
   module_init 从这个例子可以看出，所谓的小任务机制是为下半部函数的执行提供了一种执行机制，也就是说，推迟处理的事情是由tasklet_handler实现，何时执行，经由小任务机制封装后交给内核去处理。