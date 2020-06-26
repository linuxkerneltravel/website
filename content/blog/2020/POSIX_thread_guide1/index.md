---
title: "Posix线程编程指南（1）"
date: 2020-06-27T01:42:28+08:00
author: "作者：helight 编辑：马明慧"
keywords: ["线程","系统调用"]
categories : ["系统调用"]
banner : "img/blogimg/ljrimg8.jpg"
summary : "这是一个关于Posix线程编程的专栏。作者在阐明概念的基础上，将向您详细讲述Posix线程库API。本文是第一篇将向您讲述线程的创建与取消。"
---

### Posix线程编程指南(1)

2001 年 10 月 01 日

这是一个关于Posix线程编程的专栏。作者在阐明概念的基础上，将向您详细讲述Posix线程库API。本文是第一篇将向您讲述线程的创建与取消。



#### 线程创建



##### 1.1 线程与进程

相对进程而言，线程是一个更加接近于执行体的概念，它可以与同进程中的其他线程共享数据，但拥有自己的栈空间，拥有独立的执行序列。在串行程序基础上引入线程和进程是为了提高程序的并发度，从而提高程序运行效率和响应时间。线程和进程在使用上各有优缺点：线程执行开销小，但不利于资源的管理和保护；而进程正相反。同时，线程适合于在SMP机器上运行，而进程则可以跨机器迁移。

##### 1.2 创建线程

POSIX通过pthread_create()函数创建线程，API定义如下：int  pthread_create(pthread_t  *  thread, pthread_attr_t * attr, void * (*start_routine)(void *), void * arg)与fork()调用创建一个进程的方法不同，pthread_create()创建的线程并不具备与主线程（即调用pthread_create()的线程）同样的执行序列，而是使其运行start_routine(arg)函数。thread返回创建的线程ID，而attr是创建线程时设置的线程属性（见下）。pthread_create()的返回值表示线程创建是否成功。尽管arg是void类型的变量，但它同样可以作为任意类型的参数传给start_routine()函数；同时，start_routine()可以返回一个void 类型的返回值，而这个返回值也可以是其他类型，并由pthread_join()获取。

##### 1.3 线程创建属性

pthread_create()中的attr参数是一个结构指针，结构中的元素分别对应着新线程的运行属性，主要包括以下几项：detachstate，表示新线程是否与进程中其他线程脱离同步，如果置位则新线程不能用pthread_join()来同步，且在退出时自行释放所占用的资源。缺省为PTHREAD_CREATE_JOINABLE状态。这个属性也可以在线程创建并运行以后用pthread_detach()来设置，而一旦设置为PTHREAD_CREATE_DETACH状态（不论是创建时设置还是运行时设置）则不能再恢复到PTHREAD_CREATE_JOINABLE状态。schedpolicy，表示新线程的调度策略，主要包括SCHED_OTHER（正常、非实时）、SCHED_RR（实时、轮转法）和 SCHED_FIFO（实时、先入先出）三种，缺省为SCHED_OTHER，后两种调度策略仅对超级用户有效。运行时可以用过 pthread_setschedparam()来改变。schedparam，一个struct sched_param结构，目前仅有一个sched_priority整型变量表示线程的运行优先级。这个参数仅当调度策略为实时（即SCHED_RR或SCHED_FIFO）时才有效，并可以在运行时通过pthread_setschedparam()函数来改变，缺省为0。inheritsched，有两种值可供选择：PTHREAD_EXPLICIT_SCHED和PTHREAD_INHERIT_SCHED，前者表示新线程使用显式指定调度策略和调度参数（即attr中的值），而后者表示继承调用者线程的值。缺省为PTHREAD_EXPLICIT_SCHED。scope，表示线程间竞争CPU的范围，也就是说线程优先级的有效范围。POSIX的标准中定义了两个值：PTHREAD_SCOPE_SYSTEM和PTHREAD_SCOPE_PROCESS，前者表示与系统中所有线程一起竞争CPU时间，后者表示仅与同进程中的线程竞争CPU。目前LinuxThreads仅实现了PTHREAD_SCOPE_SYSTEM一值。pthread_attr_t结构中还有一些值，但不使用pthread_create()来设置。为了设置这些属性，POSIX定义了一系列属性设置函数，包括pthread_attr_init()、pthread_attr_destroy()和与各个属性相关的pthread_attr_get/pthread_attr_set函数。

##### 1.4 线程创建的Linux实现

我们知道，Linux的线程实现是在核外进行的，核内提供的是创建进程的接口do_fork()。内核提供了两个系统调用clone()和fork()，最终都用不同的参数调用do_fork()核内API。当然，要想实现线程，没有核心对多进程（其实是轻量级进程）共享数据段的支持是不行的，因此，do_fork()提供了很多参数，包括CLONE_VM（共享内存空间）、CLONE_FS（共享文件系统信息）、CLONE_FILES（共享文件描述符表）、CLONE_SIGHAND（共享信号句柄表）和CLONE_PID（共享进程ID，仅对核内进程，即0号进程有效）。当使用fork系统调用时，内核调用do_fork()不使用任何共享属性，进程拥有独立的运行环境，而使用pthread_create()来创建线程时,则最终设置了所有这些属性来调用clone()，而这些参数又全部传给核内的do_fork()，从而创建的"进程"拥有共享的运行环境，只有栈是独立的，由clone()传入。Linux线程在核内是以轻量级进程的形式存在的，拥有独立的进程表项，而所有的创建、同步、删除等操作都在核外pthread库中进行。pthread库使用一个管理线程（pthread_manager()，每个进程独立且唯一）来管理线程的创建和终止，为线程分配线程ID，发送线程相关的信号（比如Cancel），而主线程（pthread_create()）的调用者则通过管道将请求信息传给管理线程。



#### 回页首线程取消



##### 2.1 线程取消的定义

一般情况下，线程在其主体函数退出的时候会自动终止，但同时也可以因为接收到另一个线程发来的终止（取消）请求而强制终止。

##### 2.2 线程取消的语义

线程取消的方法是向目标线程发Cancel信号，但如何处理Cancel信号则由目标线程自己决定，或者忽略、或者立即终止、或者继续运行至Cancelation-point（取消点），由不同的Cancelation状态决定。线程接收到CANCEL信号的缺省处理（即pthread_create()创建线程的缺省状态）是继续运行至取消点，也就是说设置一个CANCELED状态，线程继续运行，只有运行至Cancelation-point的时候才会退出。

##### 2.3 取消点

根据POSIX标准，pthread_join()、pthread_testcancel()、pthread_cond_wait()、 
pthread_cond_timedwait()、sem_wait()、sigwait()等函数以及read()、write()等会引起阻塞的系统调用都是Cancelation-point，而其他pthread函数都不会引起Cancelation动作。但是pthread_cancel的手册页声称，由于LinuxThread库与C库结合得不好，因而目前C库函数都不是Cancelation-point；但CANCEL信号会使线程从阻塞的系统调用中退出，并置EINTR错误码，因此可以在需要作为Cancelation-point的系统调用前后调用
pthread_testcancel()，从而达到POSIX标准所要求的目标，即如下代码段：

```c
pthread_testcancel();
retcode = read(fd, buffer, length);
pthread_testcancel();
```



##### 2.4 程序设计方面的考虑

如果线程处于无限循环中，且循环体内没有执行至取消点的必然路径，则线程无法由外部其他线程的取消请求而终止。因此在这样的循环体的必经路径上应该加入pthread_testcancel()调用。

##### 2.5 与线程取消相关的pthread函数

int pthread_cancel(pthread_t thread)发送终止信号给thread线程，如果成功则返回0，否则为非0值。发送成功并不意味着thread会终止。int pthread_setcancelstate(int state, int *oldstate)设置本线程对Cancel信号的反应，state有两种值：PTHREAD_CANCEL_ENABLE（缺省）和 PTHREAD_CANCEL_DISABLE，分别表示收到信号后设为CANCLED状态和忽略CANCEL信号继续运行；old_state如果不为NULL则存入原来的Cancel状态以便恢复。
int pthread_setcanceltype(int type, int *oldtype)设置本线程取消动作的执行时机，type由两种取值：PTHREAD_CANCEL_DEFFERED和 PTHREAD_CANCEL_ASYCHRONOUS，仅当Cancel状态为Enable时有效，分别表示收到信号后继续运行至下一个取消点再退出和立即执行取消动作（退出）；oldtype如果不为NULL则存入运来的取消动作类型值。void pthread_testcancel(void)检查本线程是否处于Canceld状态，如果是，则进行取消动作，否则直接返回。

##### 关于作者

杨沙洲，男，现攻读国防科大计算机学院计算机软件方向博士学位。您可以通过电子邮件 pubb@163.net跟他联系。

##### 版权声明

本文仅代表作者观点，不代表本站立场。
本文系作者授权发表，未经许可，不得转载。