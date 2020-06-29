---
title: "中断处理的工作队列机制－原来如此"
date: 2020-06-28T18:37:57+08:00
keywords: ["中断处理","工作队列"]
categories : ["中断"]
banner : "img/blogimg/tasklet_hds.png"
author: "作者：helight 编辑：贺东升"
summary : "工作队列（work queue）是另外一种将工作推后执行的形式 ，它和我们前面讨论的所有其他形式都有不同。工作队列可以把工作推后，交由一个内核线程去执行，也就是说，这个下半部分可以在进程上下文中执行。这样，通过工作队列执行的代码能占尽进程上下文的所有优势。最重要的就是工作队列允许被重新调度甚至是睡眠。"
---

#### 中断处理的工作队列机制－原来如此

工作队列（`work queue`）是另外一种将工作推后执行的形式 ，它和我们前面讨论的所有其他形式都有不同。工作队列可以把工作推后，交由一个内核线程去执行，也就是说，这个下半部分可以在进程上下文中执行。这样，通过工作队列执行的代码能占尽进程上下文的所有优势。最重要的就是工作队列允许被重新调度甚至是睡眠。

那么，什么情况下使用工作队列，什么情况下使用`tasklet`。如果推后执行的任务需要睡眠，那么就选择工作队列。如果推后执行的任务不需要睡眠，那么就选择`tasklet`。另外，如果需要用一个可以重新调度的实体来执行你的下半部处理，也应该使用工作队列。它是唯一能在进程上下文运行的下半部实现的机制，也只有它才可以睡眠。这意味着在需要获得大量的内存时、在需要获取信号量时，在需要执行阻塞式的I/O操作时，它都会非常有用。如果不需要用一个内核线程来推后执行工作，那么就考虑使用`tasklet`。



##### 1.工作、工作队列和工作者线程

如前所述，我们把推后执行的任务叫做工作（`work`），描述它的数据结构为`work_struct`，这些工作以队列结构组织成工作队列（`workqueue`），其数据结构为`workqueue_struct`，而工作线程就是负责执行工作队列中的工作。系统默认的工作者线程为events,自己也可以创建自己的工作者线程。



##### 2.表示工作的数据结构

工作用<`linux/workqueue.h`>中定义的`work_struct`结构表示：

```c
struct  work_struct{

    unsigned long pending;          /* 这个工作正在等待处理吗？*/

    struct list_head entry;         /* 连接所有工作的链表 */ 

    void (*func) (void *);          /* 要执行的函数 */

    void *data;                     /* 传递给函数的参数 */

    void *wq_data;                  /* 内部使用 */

    struct timer_list timer;        /* 延迟的工作队列所用到的定时器 */

};
```

这些结构被连接成链表。当一个工作者线程被唤醒时，它会执行它的链表上的所有工作。工作被执行完毕，它就将相应的`work_struct`对象从链表上移去。当链表上不再有对象的时候，它就会继续休眠。



##### 3.创建推后的工作

要使用工作队列，首先要做的是创建一些需要推后完成的工作。可以通过`DECLARE_WORK`在编译时静态地建该结构：

```c
DECLARE_WORK(name, void (*func) (void *), void *data);
```

这样就会静态地创建一个名为`name`，待执行函数为`func`，参数为data的`work_struct`结构。

同样，也可以在运行时通过指针创建一个工作：

```c
INIT_WORK(struct work_struct *work, woid(*func) (void *), void *data);
```

这会动态地初始化一个由work指向的工作。



##### 4.工作队列中待执行的函数

工作队列待执行的函数原型是：

```c
void work_handler(void *data)
```

这个函数会由一个工作者线程执行，因此，函数会运行在进程上下文中。默认情况下，允许响应中断，并且不持有任何锁。如果需要，函数可以睡眠。需要注意的是，尽管该函数运行在进程上下文中，但它不能访问用户空间，因为内核线程在用户空间没有相关的内存映射。通常在系统调用发生时，内核会代表用户空间的进程运行，此时它才能访问用户空间，也只有在此时它才会映射用户空间的内存。



5.对工作进行调度

现在工作已经被创建，我们可以调度它了。想要把给定工作的待处理函数提交给缺省的`events`工作线程，只需调用

```c
schedule_work(&work)；
```

`work`马上就会被调度，一旦其所在的处理器上的工作者线程被唤醒，它就会被执行。

有时候并不希望工作马上就被执行，而是希望它经过一段延迟以后再执行。在这种情况下，可以调度它在指定的时间执行：

```c
schedule_delayed_work(&work, delay);
```

这时，&work指向的`work_struct`直到`delay`指定的时钟节拍用完以后才会执行。



##### 6.工作队列的简单应用

```c
#include <linux/module.h> 
#include <linux/init.h> 
#include <linux/workqueue.h> 

static struct workqueue_struct *queue = NULL; 
static struct work_struct work; 

static void work_handler(struct work_struct *data) {     
    printk(KERN_ALERT "work handler function.\n"); 
} 

static int __init test_init(void) {     
    queue = create_singlethread_workqueue("helloworld"); /*创建一个单线程的工作队列*/     
    
    if (!queue)         
        goto err;     
    
    INIT_WORK(&work, work_handler);     
    schedule_work(&work);     
    return 0; 
    
    err:     
    	return -1; 
} 
static void __exit test_exit(void) {     
    destroy_workqueue(queue); 
} 

MODULE_LICENSE("GPL"); 
module_init(test_init); 
module_exit(test_exit);
```

