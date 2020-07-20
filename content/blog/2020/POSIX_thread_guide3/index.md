---
title: "Posix线程编程指南（3）"
date: 2020-07-20T18:35:20+08:00
author: "作者：HELIGHT 编辑：马明慧"
keywords: ["进程","系统调用"]
categories : ["系统调用"]
banner : "img/blogimg/ljrimg21.jpg"
summary : "这是一个关于Posix线程编程的专栏。作者在阐明概念的基础上，将向您详细讲述Posix线程库API。本文是第三篇将向您讲述线程同步。"
---

### Posix线程编程指南(3)

**2001 年 10 月 01 日**

这是一个关于Posix线程编程的专栏。作者在阐明概念的基础上，将向您详细讲述Posix线程库API。本文是第三篇将向您讲述线程同步。

#### **互斥锁**

尽管在Posix Thread中同样可以使用IPC的信号量机制来实现互斥锁mutex功能，但显然semphore的功能过于强大了，在Posix Thread中定义了另外一套专门用于线程同步的mutex函数。

**1．创建和销毁**
有两种方法创建互斥锁，静态方式和动态方式。POSIX定义了一个宏PTHREAD_MUTEX_INITIALIZER来静态初始化互斥锁，方法如下：
`pthread_mutex_t mutex=PTHREAD_MUTEX_INITIALIZER; `
在Linux Threads实现中，`pthread_mutex_t`是一个结构，而`PTHREAD_MUTEX_INITIALIZER`则是一个结构常量。
动态方式是采用`pthread_mutex_init()`函数来初始化互斥锁，API定义如下： `int pthread_mutex_init(pthread_mutex_t *mutex, const pthread_mutexattr_t *mutexattr) `其中`mutexattr`用于指定互斥锁属性（见下），如果为NULL则使用缺省属性。`pthread_mutex_destroy()`用于注销一个互斥锁，API定义如下： `int pthread_mutex_destroy(pthread_mutex_t *mutex) `销毁一个互斥锁即意味着释放它所占用的资源，且要求锁当前处于开放状态。由于在Linux中，互斥锁并不占用任何资源，因此Linux Threads中的`pthread_mutex_destroy()`除了检查锁状态以外（锁定状态则返回EBUSY）没有其他动作。

**2． 互斥锁属性**

互斥锁的属性在创建锁的时候指定，在Linux Threads实现中仅有一个锁类型属性，不同的锁类型在试图对一个已经被锁定的互斥锁加锁时表现不同。当前（**glibc2.2.3,linux threads 0.9**）有四个值可供选择：

* `PTHREAD_MUTEX_TIMED_NP`，这是缺省值，也就是普通锁。当一个线程加锁以后，其余请求锁的线程将形成一个等待队列，并在解锁后按优先级获得锁。这种锁策略保证了资源分配的公平性。
* `PTHREAD_MUTEX_RECURSIVE_NP`，嵌套锁，允许同一个线程对同一个锁成功获得多次，并通过多次unlock解锁。如果是不同线程请求，则在加锁线程解锁时重新竞争。
* `PTHREAD_MUTEX_ERRORCHECK_NP`，检错锁，如果同一个线程请求同一个锁，则返回EDEADLK，否则与`PTHREAD_MUTEX_TIMED_NP`类型动作相同。这样就保证当不允许多次加锁时不会出现最简单情况下的死锁。
* `PTHREAD_MUTEX_ADAPTIVE_NP`，适应锁，动作最简单的锁类型，仅等待解锁后重新竞争。

**3． 锁操作**
锁操作主要包括加锁`pthread_mutex_lock()`、解锁`pthread_mutex_unlock()`和测试加锁 
`pthread_mutex_trylock()`三个，不论哪种类型的锁，都不可能被两个不同的线程同时得到，而必须等待解锁。对于普通锁和适应锁类型，解锁者可以是同进程内任何线程；而检错锁则必须由加锁者解锁才有效，否则返回EPERM；对于嵌套锁，文档和实现要求必须由加锁者解锁，但实验结果表明并没有这种限制，这个不同目前还没有得到解释。在同一进程中的线程，如果加锁后没有解锁，则任何其他线程都无法再获得锁。
`int pthread_mutex_lock(pthread_mutex_t *mutex)`
`int pthread_mutex_unlock(pthread_mutex_t *mutex)`
`int pthread_mutex_trylock(pthread_mutex_t *mutex)`
`pthread_mutex_trylock()`语义与`pthread_mutex_lock()`类似，不同的是在锁已经被占据时返回EBUSY而不是挂起等待。

**4． 其他**
POSIX线程锁机制的Linux实现都不是取消点，因此，延迟取消类型的线程不会因收到取消信号而离开加锁等待。值得注意的是，如果线程在加锁后解锁前被取消，锁将永远保持锁定状态，因此如果在关键区段内有取消点存在，或者设置了异步取消类型，则必须在退出回调函数中解锁。这个锁机制同时也不是异步信号安全的，也就是说，不应该在信号处理过程中使用互斥锁，否则容易造成死锁。



#### **条件变量**

条件变量是利用线程间共享的全局变量进行同步的一种机制，主要包括两个动作：一个线程等待"条件变量的条件成立"而挂起；另一个线程使"条件成立"（给出条件成立信号）。为了防止竞争，条件变量的使用总是和一个互斥锁结合在一起。
**1． 创建和注销**
条件变量和互斥锁一样，都有静态动态两种创建方式，**静态方式**使用`PTHREAD_COND_INITIALIZER`常量，如下：
`pthread_cond_t cond=PTHREAD_COND_INITIALIZER`，**动态方式**调用`pthread_cond_init()`函数，API定义如下：`int pthread_cond_init(pthread_cond_t *cond, pthread_condattr_t *cond_attr)`尽管POSIX标准中为条件变量定义了属性，但在Linux Threads中没有实现，因此`cond_attr`值通常为NULL，且被忽略。注销一个条件变量需要调用`pthread_cond_destroy()`，只有在没有线程在该条件变量上等待的时候才能注销这个条件变量，否则返回EBUSY。因为Linux实现的条件变量没有分配什么资源，所以注销动作只包括检查是否有等待线程。API定义如下：`int pthread_cond_destroy(pthread_cond_t *cond)`

**2． 等待和激发**
`int pthread_cond_wait(pthread_cond_t *cond, pthread_mutex_t *mutex)`
`int pthread_cond_timedwait(pthread_cond_t *cond, pthread_mutex_t *mutex,const struct timespec *abstime)`

**等待条件**有两种方式：无条件等待`pthread_cond_wait()`和计时等待`pthread_cond_timedwait()`，其中计时等待方式如果在给定时刻前条件没有满足，则返回ETIMEOUT，结束等待，其中abstime以与time()系统调用相同意义的绝对时间形式出现，0表示格林尼治时间1970年1月1日0时0分0秒。无论哪种等待方式，都必须和一个互斥锁配合，以防止多个线程同时请求`pthread_cond_wait()`（或`pthread_cond_timedwait()`，下同）的竞争条件（Race Condition）。`mutex`互斥锁必须是普通锁（`PTHREAD_MUTEX_TIMED_NP`）或者适应锁（`PTHREAD_MUTEX_ADAPTIVE_NP`），且在调用`pthread_cond_wait()`前必须由本线程加锁（`pthread_mutex_lock()`），而在更新条件等待队列以前，`mutex`保持锁定状态，并在线程挂起进入等待前解锁。在条件满足从而离开`pthread_cond_wait()`之前，`mutex`将被重新加锁，以与进入`pthread_cond_wait()`前的加锁动作对应。

**激发条件**有两种形式，`pthread_cond_signal()`激活一个等待该条件的线程，存在多个等待线程时按入队顺序激活其中一个；而`pthread_cond_broadcast()`则激活所有等待线程。

**3． 其他**
`pthread_cond_wait()` 和`pthread_cond_timedwait()`都被实现为取消点，因此，在该处等待的线程将立即重新运行，在重新锁定`mutex`后离开`pthread_cond_wait()`，然后执行取消动作。也就是说如果`pthread_cond_wait()`被取消，`mutex`是保持锁定状态的，因而需要定义退出回调函数来为其解锁。

以下示例集中演示了互斥锁和条件变量的结合使用，以及取消对于条件等待动作的影响。在例子中，有两个线程被启动，并等待同一个条件变量，如果不使用退出回调函数（见范例中的注释部分），则`tid2`将在`pthread_mutex_lock()`处永久等待。如果使用回调函数，则`tid2`的条件等待及主线程的条件激发都能正常工作。

```c
#include 
#include
#include 
pthread_mutex_t mutex; 
pthread_cond_t cond; 
void * child1(void *arg) 
{ 
    pthread_cleanup_push(pthread_mutex_unlock,&mutex); /* comment 1 */ 
	while(1)
	{ 
   		printf("thread 1 get running n"); 
		printf("thread 1 pthread_mutex_lock returns %dn", pthread_mutex_lock(&mutex)); 	
        pthread_cond_wait(&cond,&mutex); 
    	printf("thread 1 condition appliedn"); 
    	pthread_mutex_unlock(&mutex); sleep(5); 
	} 
	pthread_cleanup_pop(0); /* comment 2 */ 
} 
void *child2(void *arg) 
{ 
    while(1)
    { 
        sleep(3); /* comment 3 */
        printf("thread 2 get running.n"); 
        printf("thread 2 pthread_mutex_lock returns %dn", pthread_mutex_lock(&mutex)); 		
        pthread_cond_wait(&cond,&mutex); printf("thread 2 condition appliedn"); 				
        pthread_mutex_unlock(&mutex); sleep(1); 
    } 
} 
int main(void) 
{ 
    int tid1,tid2; 	
    printf("hello, condition variable testn");
    pthread_mutex_init(&mutex,NULL); 
    pthread_cond_init(&cond,NULL); pthread_create(&tid1,NULL,child1,NULL); 					
    pthread_create(&tid2,NULL,child2,NULL); 
    do
    { 
        sleep(2); /* comment 4 */ 					
        pthread_cancel(tid1); /* comment 5 */ 
        sleep(2); /* comment 6 */ 					
        pthread_cond_signal(&cond);
    }
    while(1); 
    sleep(100);
    pthread_exit(0); 
}
```

如果不做注释5的`pthread_cancel()`动作，即使没有那些`sleep()`延时操作，`child1`和`child2`都能正常工作。注释3和注释4的延迟使得`child1`有时间完成取消动作，从而使`child2`能在`child1`退出之后进入请求锁操作。如果没有注释1和注释2的回调函数定义，系统将挂起在`child2`请求锁的地方；而如果同时也不做注释3和注释4的延时，`child2`能在`child1`完成取消动作以前得到控制，从而顺利执行申请锁的操作，但却可能挂起在`pthread_cond_wait()`中，因为其中也有申请`mutex`的操作。`child1`函数给出的是标准的条件变量的使用方式：回调函数保护，等待条件前锁定，`pthread_cond_wait()`返回后解锁。条件变量机制不是异步信号安全的，也就是说，在信号处理函数中调用`pthread_cond_signal()`或者`pthread_cond_broadcast()`很可能引起死锁。

#### **信号灯**

信号灯与互斥锁和条件变量的主要不同在于"灯"的概念，灯亮则意味着资源可用，灯灭则意味着不可用。如果说后两中同步方式侧重于"等待"操作，即资源不可用的话，信号灯机制则侧重于点灯，即告知资源可用；没有等待线程的解锁或激发条件都是没有意义的，而没有等待灯亮的线程的点灯操作则有效，且能保持灯亮状态。当然，这样的操作原语也意味着更多的开销。

信号灯的应用除了灯亮/灯灭这种二元灯以外，也可以采用大于1的灯数，以表示资源数大于1，这时可以称之为多元灯。
**1． 创建和注销**
POSIX信号灯标准定义了有名信号灯和无名信号灯两种，但LinuxThreads的实现仅有无名灯，同时有名灯除了总是可用于多进程之间以外，在使用上与无名灯并没有很大的区别，因此下面仅就无名灯进行讨论。
`int sem_init(sem_t *sem, int pshared, unsigned int value)`
这是创建信号灯的API，其中`value`为信号灯的初值，`pshared`表示是否为多进程共享而不仅仅是用于一个进程。Linux Threads没有实现多进程共享信号灯，因此所有非0值的`pshared`输入都将使`sem_init()`返回-1，且置errno为ENOSYS。初始化好的信号灯由`sem`变量表征，用于以下点灯、灭灯操作。
`int sem_destroy(sem_t * sem)`被注销的信号灯`sem`要求已没有线程在等待该信号灯，否则返回-1，且置errno为EBUSY。除此之外，Linux Threads的信号灯注销函数不做其他动作。

**2． 点灯和灭灯**
`int sem_post(sem_t * sem)`
点灯操作将信号灯值原子地加1，表示增加一个可访问的资源。
`int sem_wait(sem_t * sem)`
`int sem_trywait(sem_t * sem)`
`sem_wait()`为等待灯亮操作，等待灯亮（信号灯值大于0），然后将信号灯原子地减1，并返回。`sem_trywait()`为`sem_wait()`的非阻塞版，如果信号灯计数大于0，则原子地减1并返回0，否则立即返回-1，errno置为EAGAIN。

**3． 获取灯值**
`int sem_getvalue(sem_t * sem, int * sval)`
读取`sem`中的灯计数，存于`*sval`中，并返回0。

**4． 其他**
`sem_wait()`被实现为取消点，而且在支持原子"比较且交换"指令的体系结构上，`sem_post()`是唯一能用于异步信号处理函数的POSIX异步信号安全的API。

#### **异步信号**

由于Linux Threads是在核外使用核内轻量级进程实现的线程，所以基于内核的异步信号操作对于线程也是有效的。但同时，由于异步信号总是实际发往某个进程，所以无法实现POSIX标准所要求的"信号到达某个进程，然后再由该进程将信号分发到所有没有阻塞该信号的线程中"原语，而是只能影响到其中一个线程。

POSIX异步信号同时也是一个标准C库提供的功能，主要包括信号集管理（sigemptyset()、sigfillset()、 
sigaddset()、sigdelset()、sigismember()等）、信号处理函数安装（sigaction()）、信号阻塞控制（sigprocmask()）、被阻塞信号查询（sigpending()）、信号等待(sigsuspend())等，它们与发送信号的kill()
等函数配合就能实现进程间异步信号功能。Linux Threads围绕线程封装了sigaction()和raise()，本节集中讨论 
Linux Threads中扩展的异步信号函数，包括pthread_sigmask()、pthread_kill()和sigwait()三个函数。毫无疑问，所有POSIX异步信号函数对于线程都是可用的。
`int pthread_sigmask(int how, const sigset_t *newmask, sigset_t *oldmask)`
设置线程的信号屏蔽码，语义与sigprocmask()相同，但对不允许屏蔽的Cancel信号和不允许响应的Restart信号进行了保护。被屏蔽的信号保存在信号队列中，可由`sigpending()`函数取出。
`int pthread_kill(pthread_t thread, int signo)`
向thread号线程发送signo信号。实现中在通过thread线程号定位到对应进程号以后使用kill()系统调用完成发送。
`int sigwait(const sigset_t *set, int *sig)`挂起线程，等待set中指定的信号之一到达，并将到达的信号存入`*sig`中。POSIX标准建议在调用sigwait()等待信号以前，进程中所有线程都应屏蔽该信号，以保证仅有sigwait()的调用者获得该信号，因此，对于需要等待同步的异步信号，总是应该在创建任何线程以前调用
pthread_sigmask()屏蔽该信号的处理。而且，调用sigwait()期间，原来附接在该信号上的信号处理函数不会被调用。如果在等待期间接收到Cancel信号，则立即退出等待，也就是说sigwait()被实现为取消点。

**其他同步方式**
除了上述讨论的同步方式以外，其他很多进程间通信手段对于Linux Threads也是可用的，比如基于文件系统的IPC（管道、Unix域Socket等）、消息队列（Sys.V或者Posix的）、System V的信号灯等。只有一点需要注意，Linux Threads在核内是作为共享存储区、共享文件系统属性、共享信号处理、共享文件描述符的独立进程看待的。

#### **关于作者**

杨沙洲，男，现攻读国防科大计算机学院计算机软件方向博士学位。您可以通过电子邮件 pubb@163.net跟他联系。