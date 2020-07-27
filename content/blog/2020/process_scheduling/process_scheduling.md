---
title: "Linux进程调度中队列的使用"
date: 2020-07-27T14:40:30+08:00
author: "作者：王聪 编辑：张孝家"
keywords: ["队列","调度"]
categories : ["经验交流"]
banner : "img/blogimg/zxj0.jpg"
summary : "Linux内核中大量使用了队列，这里仅列举它在进程调度中的几处应用。Linux内核中的队列是以双链表的形式连接起来的，include/linux/list.h中定义了队列并提供了一些接口，详细的介绍可以参考[1]中的附录。"
---

Linux内核中大量使用了队列，这里仅列举它在进程调度中的几处应用。Linux内核中的队列是以双链表的形式连接起来的，include/linux/list.h中定义了队列并提供了一些接口，详细的介绍可以参考**[1]**中的附录。

Linux中的进程有如下几个主要状态：

| ***进程状态***       | ***说明***                             |
| :------------------- | :------------------------------------- |
| TASK_RUNNING         | 进程正在运行或将要被运行。             |
| TASK_INTERRUPTIBLE   | 进程正在睡眠，等待某个条件的完成。     |
| TASK_UNINTERRUPTIBLE | 深度睡眠，不会被信号打扰。             |
| TASK_STOPPED         | 进程运行被停止。                       |
| TASK_TRACED          | 进程被调试程序停止，被另一个进程跟踪。 |

两个额外的状态是EXIT_ZOMBIE和EXIT_DEAD，表示进程处于僵死状态还是真正死亡。处于僵死状态的进程会等待其父进程的收养（否则就会被init进程收养），而真正死亡的进程会被直接删除。

状态为TASK_RUNNING的进程都会被放入运行队列（runqueue）中，这是通过task_struct（定义在include/linux/sched.h）中的run_list成员来链接的。不过，为了让内核每次都能选取优先级最合适的进程，Linux为每个优先级构建了一个queue。这是通过struct prio_array来实现的，struct prio_array的定义在kernel/sched.c，大致如下：

```
struct prio_array 
int nr_active;
unsigned long bitmap[BITMAP_SIZE];
struct list_head queue[MAX_PRIO];
};
```

queue成员就是队列数组。每个CPU有各自的runqueue，每一个runqueue又有包含两个prio_array，一个是活动队列，一个是时间片耗尽的队列。当运行队列空时，内核便会交换两个队列的指针，原来的耗尽队列就成了新的活动队列！这和prio_array中的bitmap是决定调度算法为O(1)的关键。


状态为TASK_STOPPED，EXIT_ZOMBIE或EXIT_DEAD的进程不会被放入专门的队列中，它们直接通过pid或者通过父进程的孩子队列来访问。


TASK_INTERRUPTIBLE和TASK_UNINTERRUPTIBLE状态的进程会被放入等待队列。不同的是，每个等待事件都会有一个等待队列，该队列中的进程等待同一事件的完成。（“事件”一个动态的过程，不好通过具体的结构来定义一个“事件”。这里等待一个事件就是查看某个条件是否为真，比如某个标志变量是否为真。）wait_queue_head_t的定义在include/linux/wait.h中，如下：

```
typedef struct _ _wait_queue_head {
spinlock_t lock;
struct list_head task_list;
}wait_queue_head_t;
```

wait_queue_t的定义如下：
```
typedef struct _ _wait_queue {
unsigned int flags;
struct task_struct * task;
wait_queue_func_t func;
struct list_head task_list;
}wait_queue_t;
```

进入等待状态的接口有两类：

```
prepare_to_wait*()/finish_wait()
wait_event*()
```

其实```wait_event*()```内部也是调用```prepare_to_wait*()```，把它放入一个循环中。而且```wait_event*()```在事件完成时会自动调用```finish_wait()```。决定使用哪个要看情况而定。（```sleep_on*()```是遗弃的接口，现在已经不再使用，虽然还支持。）等待队列中的进程有两种，一种是exclusive的进程，另一种是nonexclusive的进程。所谓exclusive是指唤醒的进程等待的资源是互斥的，每次只唤醒一个（唤醒多个也可以，不过最后还是只有一个会被唤醒，其余的又被重新添加到等待队列中，这样效率会大打折扣）。一般，等待函数会把进程设为nonexclusive和uninterruptible，带“interruptible”的会专门指定状态为interruptible；而带“timeout”的会在超时后退出，因为它会调用schedule_timeout()；带“exclusive”的则会把进程设为exclusive。

唤醒的接口虽然只有```wake_up*()```，但它内部也分成好几种。带“interruptible”的唤醒函数只会唤醒状态是TASK_INTERRUPTIBLE的进程，而不带的则会唤醒TASK_INTERRUPTIBLE和TASK_UNINTERRUPTIBLE的进程；所有唤醒函数都会把等待同一事件的nonexclusive进程全部唤醒，或者把其中一个exclusive的进程唤醒；而带“nr”的则会唤醒指定个数的exclusive的进程，带“all”的会把全部exclusive进程唤醒。带“sync”会忽略优先级的检查，高优先级的进程可能会被延迟。最后，持有自旋锁时只能调用wait_up_locked()。

进程管理是Linux内核中最关键的部分，它的性能几乎直接决定着内核的好坏，其中使用的一些设计和算法非常复杂。这里仅仅是介绍了队列在其中的使用情况，更深入的探索还有待继续去探索。

#### 参考资料

1. *Linux Kernel Development, Second Edition*, Robert Love, Sam Publishing.

2. *Understanding the Linux Kernel, 3rd Edition*, Daniel P. Bovet, Marco Cesati, O'Reilly.

3. *The Linux Kernel Primer: A Top-Down Approach for x86 and PowerPC Architectures*, Claudia Salzberg Rodriguez, Gordon Fischer, Steven Smolski, Prentice Hall PTR.