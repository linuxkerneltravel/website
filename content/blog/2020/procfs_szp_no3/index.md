---
title: "proc文件系统探索 之 以数字命名的目录[三]"
date: 2020-12-19T10:46:11+08:00
author: "孙张品转"
keywords: ["proc",]
categories : ["走进内核"]
banner : "img/blogimg/cxl_1.jpg"
summary : "fd目录包含了所有该进程使用的文件描述符,而fdinfo目录包含的是对应的fd目录中进程打开文件的操作权限。"
---

7．fd目录 fdinfo目录 

fd目录包含了所有该进程使用的文件描述符,而fdinfo目录包含的是对应的fd目录中进程打开文件的操作权限。

```

niutao@niutao-desktop:/proc/6772/fd$ ls -l lrwx------ 1 niutao niutao 64 2008-10-22 21:32 0 -> /dev/pts/5 lrwx------ 1 niutao niutao 64 2008-10-22 21:32 1 -> /dev/pts/5 lrwx------ 1 niutao niutao 64 2008-10-22 21:32 2 -> /dev/pts/5 niutao@niutao-desktop:/proc/6772/fd$ cd ../fdinfo/ niutao@niutao-desktop:/proc/6772/fdinfo$ cat 0 pos: 0 flags: 02

```

我们可以看出fd目录中包含的是进程打开文件的链接，这里我们就可以看到我们经常提到的标准输入(0)，标准输出(1)，标准错误输出(2)。那么 fdinfo中包含的文件的含义，我们可以从这两个方面探索。一个是内核中的 proc_fd_info函数(/fs/proc/base.c)，fdinfo目录中的文件中的内容正是由这个函数写的，而flags对应的是文件结 构体(struct file)的f_flags域(知识我呢件的访问权限)。另一个是我们可以通过查看fd目录中包含的符号链接文件指向的文件的权限得知其(fdinfo目 录中文件内容)含义：

```
niutao@niutao-desktop:/proc/6772/fd$ ls -l /dev/pts/5 crw--w---- 1 niutao tty 136, 5 2008-10-22 21:32 /dev/pts/5
```

"flags:02"表示文件访问权限是O_RDWR（可读可写），而fd中符号链接文件指向的文件/dev/pts/5对用户的权限也是可读可写。 

8．root符号链接文件 该文件指向的是根目录(/)。

9．stat文件 该文件的内容反应的是该进程的PCB(task_struct结构)的一些数据域的信息。下面我们来具体看一下它的含义。首先我们在终端上启动gedit程 序，然后使用系统监视器(gnome-system-monitor)查看gedit进程的pid为11942，然后我们读取它的stat文件

```
niutao@niutao-desktop:/proc/11942$ cat stat 11942 (gedit) S 7293 11942 7293 34820 11942 4202496 5017 0 0 0 80 10 0 0 20 0 1 0 1292037 61636608 4481 4294967295 134512640 135099420 3216990608 3216990068 3085931536 0 0 4096 0 0 0 0 17 0 0 0 0 0 0

```

在内核中，该文件的内容由do_task_stat函数(fs/proc/array.c)写。主要操作是：
```c
sprintf(buffer, "%d (%s) %c %d %d %d %d %d %u %lu \ %lu %lu %lu %lu %lu %ld %ld %ld %ld %d 0 %llu %lu %ld %lu %lu %lu %lu %lu \ %lu %lu %lu %lu %lu %lu %lu %lu %d %d %u %u %llu %lu %ld\n", task_pid_nr_ns(task, ns), /*进程(包括轻量级进程，即线程)号(task->pid)*/ 
tcomm, /*应用程序的名字(task->comm)*/ 
state,/*进程的状态信息(task->state),具体参见http://blog.chinaunix.net/u2/73528/showart_1106510.html*/ 
ppid,/*父进程ID*/ 
pgid,/*线程组ID*/ 
sid,/*会话组ID*/ 
tty_nr,/*该进程的tty终端的设备号，INT（34817/256）=主设备号，（34817-主设备号）=次设备号*/ tty_pgrp,/*终端的进程组号，当前运行在该进程所在终端的前台进程(包括shell 应用程序)的PID*/ task->flags,/*进程标志位，查看该进程的特性(定义在/include/kernel/sched.h中)*/ 
min_flt,/*累计进程的次缺页数（Copy on　Write页和匿名页）*/ 
cmin_flt,/*该进程所有的子进程发生的次缺页的次数*/ 
maj_flt,/*主缺页数（从映射文件或交换设备读入的页面数）*/ 
cmaj_flt,/*该进程所有的子进程发生的主缺页的次数*/ 
cputime_to_clock_t(utime),/*该进程在用户态运行的时间，单位为jiffies*/ 
cputime_to_clock_t(stime),/*该进程在核心态运行的时间，单位为jiffies*/ 
cputime_to_clock_t(cutime),/*该进程所有的子进程在用户态运行的时间总和，单位为jiffies*/ cputime_to_clock_t(cstime),/*该进程所有的子进程在内核态运行的时间的总和，单位为jiffies*/ priority,/*进程的动态优先级*/ 
nice,/*进程的静态优先级*/
num_threads,/*该进程所在的线程组里线程的个数*/
start_time,/*该进程创建的时间*/
vsize,/*该进程的虚拟地址空间大小*/
mm ? get_mm_rss(mm) : 0,/*该进程当前驻留物理地址空间的大小*/
rsslim,/*该进程能驻留物理地址空间的最大值*/
mm ? mm->start_code : 0,/*该进程在虚拟地址空间的代码段的起始地址*/
mm ? mm->end_code : 0,/*该进程在虚拟地址空间的代码段的结束地址*/
mm ? mm->start_stack : 0,/*该进程在虚拟地址空间的栈的结束地址*/. esp,/*esp(32 位堆栈指针) 的当前值, 与在进程的内核堆栈页得到的一致*/
eip,/*指向将要执行的指令的指针, EIP(32 位指令指针)的当前值*/. /* The signal information here is obsolete. * It must be decimal for Linux 2.0 compatibility. * Use /proc/#/status for real-time signals. */ task->pending.signal.sig[0] & 0x7fffffffUL,/*待处理信号的位图，记录发送给进程的普通信号*/
task->blocked.sig[0] & 0x7fffffffUL,/*阻塞信号的位图*/
sigign .sig[0] & 0x7fffffffUL,/*忽略的信号的位图*/
sigcatch .sig[0] & 0x7fffffffUL,/*被俘获的信号的位图*/
wchan,/*如果该进程是睡眠状态，该值给出调度的调用点*/
0UL,/*被swapped的页数,当前没用*/
0UL,/*所有子进程被swapped的页数的和，当前没用*/
task->exit_signal,/*该进程结束时，向父进程所发送的信号*/
task_cpu(task),/*运行在哪个CPU上*/
task->rt_priority,/*实时进程的相对优先级别*/
task->policy,/*进程的调度策略，0=非实时进程，1=FIFO实时进程；2=RR实时进程*/
(unsigned long long)delayacct_blkio_ticks(task),/**/. cputime_to_clock_t(gtime),/**/. cputime_to_clock_t(cgtime));/**/
```

由以上解释我们可以知道该进程的pid为11942,可执行程序名为gedit，当前正处于睡眠状态，其父进程pid为7293，按理说应该是一个终端(因为我们是在终端上启动gedit的)，那么我们来验证一下：

```
bashniutao@niutao-desktop:/proc/7293$ cat stat 7293 (bash) S 7095 7293 7293 34820 7293 4194304 2902 40892 1 166 16 2 600 64 20 0 1 0 509398 6619136 918 4294967295 134512640 135194160 3214795824 3214792344 3086427152 0 0 3686404 1266761467 0 0 0 17 0 0 0 0 0 0
```

可以看到pid等于7293的进程其可执行程序名的确是bash，所以它就是一个终端，并且是11942(gedit)的父进程。我们接着看11942号进 程。其所在线程组id为11942，会话组id都为7293，所以如果我们关闭pid为7293的终端，则11942号进程也会被关闭。

它的tty终端下 启动的，所以终端设备号是一个有效值(如果是双击启动的，那么该项0，也就是说该程序不是在终端下启动的)。终端的进程组号为7293，该进程的标志为4202496，对应十六进制为0x402000。对于进程的标志，内核定义在/include/linux/sched.h中，都已PF_开头 （process flags），0x402000=PF_RANDOMIZE | PF_USED_MATH，表示没有设置fpu的话，这个进程在使用任何变量之前都需初始化(PF_USED_MATH)，并且该进程的虚拟地址空间是不 固定的(PF_RANDOMIZE )。接下来的4个(5017 0 0 0)是该进程的缺页管理的统计，说明该进程发生了5017次次缺页（次缺页：Copy on　Write页和匿名页）(min_flt)，并且其所有子进程没有发生次缺页(cmin_flt)，没有发生主缺页（主缺页：从映射文件或交换设备 读入的页面数）（maj_flt），并且其所有子进程没有发生住缺页（cmaj_flt）。

该进程在用户态下运行的时间是80个jiffies（在我的系 统中jiffies等于250，所以80个jiffies为20秒）(cputime_to_clock_t(utime))，该进程在内核态下运行的时 间是10个jiffies(2.5秒)(cputime_to_clock_t(stime))，该进程的所有子进程在用户态下运行的时间为 0(cputime_to_clock_t(cutime))，所有子进程在内核态下运行的时间为 0(cputime_to_clock_t(cstime))。该进程的动态优先级为20（priority），静态优先级为0（nice）。该进程所在 的线程组里的线程个数为1（num_threads）。接下来的一个"0"是直接输出的，没有含义。下面的1292037是该进程的的创建时间，说明该进 程的创建时间是开机后大约3.6小时时创建的（start_time=task->real_start_time,start_time/100 /3600=小时）。该进程的虚拟地址空间的大小是61636608B()，该进程当前主流物理内存空间的大小是4481B，能驻留物理地址空间的最大值 为4294967295B(4GB)，在虚拟地址空间的代码段的起始地址是134512640(0x8048000，一般的应用程序虚拟地址空间的代码段 的起始地址都是0x80xxxxx，可以使用objdump -d查看)，虚拟地址空间的代码段的结束地址是135099420（0x80D741C），虚拟地址空间的栈的起始地址是 3216990608（0xBFBF6190），堆栈指针的当前值为3216990068（0xBFBF5F74），可见我的系统的堆栈的扩展方向是向下 扩展（每压栈一个数，esp向下递减）。

下一条要执行的指令的地址是3085931536（0xB7EF9410）。接下来的7个（0 0 4096 0 0 0 0 ）是与信号有关的，内核注释说在这里已经没有作用，这里就不做解释。下面一个是进程退出时向父进程发送的信号，该出发送的信号为SIGCHLD(17)。 下来一个0表示该进程运行在第0个cpu上。该进程的实时程的相对优先级别为0，该进程是一个非实时进程。