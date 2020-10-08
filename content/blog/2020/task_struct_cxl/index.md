---
title: "linux中对Task_struct结构体的注释"
date: 2020-10-05T20:37:30+08:00
author: "陈小龙"
keywords: ["linux","task_struct"]
categories : ["对内核结构体task_struct的了解及使用"]
banner : "img/blogimg/cxl_13.jpg"
summary : "本文主要对linux内核中的task_struct结构体进行成员的分析，主要包括调度的数据成员、进程队列指针、进程标识、进程队列的全局变量等13个部分。"
---

task_struct注释的整体结构图

![](img/1.png)

1. 调度数据成员
(1) volatile long states;
表示进程的当前状态:
<1>TASK_RUNNING:正在运行zhi或在就绪队列run-queue中准备运行的进程，实际参与进程调dao度。
<2>TASK_INTERRUPTIBLE:处于等待队列中的进程，待资源有效时唤醒，也可由其它进程通过信号(signal)或定时中断唤醒后进入就绪队列run-queue。
 <3>TASK_UNINTERRUPTIBLE:处于等待队列中的进程，待资源有效时唤醒，不可由其它进程通过信号(signal)或定时中断唤醒。
 <4>TASK_ZOMBIE:表示进程结束但尚未消亡的一种状态(僵死状态)。此时，进程已经结束运行且释放大部分资源，但尚未释放进程控制块。
<5>TASK_STOPPED:进程被暂停，通过其它进程的信号才能唤醒。导致这种状态的原因有二，或者是对收到SIGSTOP、SIGSTP、SIGTTIN或SIGTTOU信号的反应，或者是受其它进程的ptrace系统调用的控制而暂时将CPU交给控制进程。
<6>TASK_SWAPPING: 进程页面被交换出内存的进程。
(2) unsigned long flags;
进程标志:
<1>PF_ALIGNWARN 打印“对齐”警告信息。
<2>PF_PTRACED 被ptrace系统调用监控。
<3>PF_TRACESYS 正在跟踪。
<4>PF_FORKNOEXEC 进程刚创建，但还没执行。
<5>PF_SUPERPRIV 超级用户特权。
<6>PF_DUMPCORE dumped core。
<7>PF_SIGNALED 进程被信号(signal)杀出。
<8>PF_STARTING 进程正被创建。
<9>PF_EXITING 进程开始关闭。
<10>PF_USEDFPU 该进程使用FPU(SMP only)。
<11>PF_DTRACE delayed trace (used on m68k)。
(3) long priority;
进程优先级。 Priority的值给出进程每次获取CPU后可使用的时间(按jiffies计)。优先级可通过系统调用sys_setpriorty改变(在kernel/sys.c中)。
(4) unsigned long rt_priority;
rt_priority 给出实时进程的优先级，rt_priority+1000给出进程每次获取CPU后可使用的时间(同样按jiffies计)。实时进程的优先级可通过系统 调用sys_sched_setscheduler()改变(见kernel/sched.c)。
(5) long counter;
在 轮转法调度时表示进程当前还可运行多久。在进程开始运行是被赋为priority的值，以后每隔一个tick(时钟中断)递减1，减到0时引起新一轮调 度。重新调度将从run_queue队列选出counter值最大的就绪进程并给予CPU使用权，因此counter起到了进程的动态优先级的作用 (priority则是静态优先级)。
(6) unsigned long policy;
该进程的进程调度策略，可以通过系统调用sys_sched_setscheduler()更改(见kernel/sched.c)。调度策略有:
<1>SCHED_OTHER 0 非实时进程，基于优先权的轮转法(round robin)。
<2>SCHED_FIFO 1 实时进程，用先进先出算法。
<3>SCHED_RR 2 实时进程，用基于优先权的轮转法。
2. 信号处理
(1) unsigned long signal;
进程接收到的信号。每位表示一种信号，共32种。置位有效。
(2) unsigned long blocked;
进程所能接受信号的位掩码。置位表示屏蔽，复位表示不屏蔽。
(3) struct signal_struct *sig;
因 为signal和blocked都是32位的变量，Linux最多只能接受32种信号。对每种信号，各进程可以由PCB的sig属性选择使用自定义的处理 函数，或是系统的缺省处理函数。指派各种信息处理函数的结构定义在include/linux/sched.h中。对信号的检查安排在系统调用结束后，以 及“慢速型”中断服务程序结束后。
3. 进程队列指针
(1) struct task_struct *next_task，*prev_task;
所有进程(以PCB的形式)组成一个双向链表。next_task和就是链表的前后指针。链表的头和尾都是init_task(即0号进程)。
(2) struct task_struct *next_run，*prev_run;
由正在运行或是可以运行的，其进程状态均为TASK_RUNNING的进程所组成的一个双向循环链表，即run_queue就绪队列。该链表的前后向指针用next_run和prev_run，链表的头和尾都是init_task(即0号进程)。
(3) struct task_struct *p_opptr，*p_pptr;和struct task_struct *p_cptr，*p_ysptr，*p_osptr;
以上分别是指向原始父进程(original parent)、父进程(parent)、子进程(youngest child)及新老兄弟进程(younger sibling，older sibling)的指针。
4. 进程标识
(1) unsigned short uid，gid;
uid和gid是运行进程的用户标识和用户组标识。
(2) int groups[NGROUPS];
与多数现代UNIX操作系统一样，Linux允许进程同时拥有一组用户组号。在进程访问文件时，这些组号可用于合法性检查。
(3) unsigned short euid，egid;
euid 和egid又称为有效的uid和gid。出于系统安全的权限的考虑，运行程序时要检查euid和egid的合法性。通常，uid等于euid，gid等于 egid。有时候，系统会赋予一般用户暂时拥有root的uid和gid(作为用户进程的euid和egid)，以便于进行运作。
(4) unsigned short fsuid，fsgid;
fsuid 和fsgid称为文件系统的uid和gid，用于文件系统操作时的合法性检查，是Linux独特的标识类型。它们一般分别和euid和egid一致，但在 NFS文件系统中NFS服务器需要作为一个特殊的进程访问文件，这时只修改客户进程的fsuid和fsgid。
(5) unsigned short suid，sgid;
suid和sgid是根据POSIX标准引入的，在系统调用改变uid和gid时，用于保留真正的uid和gid。
(6) int pid，pgrp，session;
进程标识号、进程的组织号及session标识号，相关系统调用(见程序kernel/sys.c)有sys_setpgid、sys_getpgid、sys_setpgrp、sys_getpgrp、sys_getsid及sys_setsid几种。
(7) int leader;
是否是session的主管，布尔量。
5. 时间数据成员
(1) unsigned long timeout;
用于软件定时，指出进程间隔多久被重新唤醒。采用tick为单位。
(2) unsigned long it_real_value，it_real_iner;
用 于itimer(interval timer)软件定时。采用jiffies为单位，每个tick使it_real_value减到0时向进程发信号SIGALRM，并重新置初值。初值由 it_real_incr保存。具体代码见kernel/itimer.c中的函数it_real_fn()。
(3) struct timer_list real_timer;
一种定时器结构(Linux共有两种定时器结构，另一种称作old_timer)。数据结构的定义在include/linux/timer.h中，相关操作函数见kernel/sched.c中add_timer()和del_timer()等。
(4) unsigned long it_virt_value，it_virt_incr;
关 于进程用户态执行时间的itimer软件定时。采用jiffies为单位。进程在用户态运行时，每个tick使it_virt_value减1，减到0时 向进程发信号SIGVTALRM，并重新置初值。初值由it_virt_incr保存。具体代码见kernel/sched.c中的函数 do_it_virt()。
(5) unsigned long it_prof_value，it_prof_incr;
同样是 itimer软件定时。采用jiffies为单位。不管进程在用户态或内核态运行，每个tick使it_prof_value减1，减到0时向进程发信号 SIGPROF，并重新置初值。初值由it_prof_incr保存。 具体代码见kernel/sched.c中的函数do_it_prof。
(6) long utime，stime，cutime，cstime，start_time;
以上分别为进程在用户态的运行时间、进程在内核态的运行时间、所有层次子进程在用户态的运行时间总和、所有层次子进程在核心态的运行时间总和，以及创建该进程的时间。
6. 信号量数据成员
(1) struct sem_undo *semundo;
进 程每操作一次信号量，都生成一个对此次操作的undo操作，它由sem_undo结构描述。这些属于同一进程的undo操作组成的链表就由semundo 属性指示。当进程异常终止时，系统会调用undo操作。sem_undo的成员semadj指向一个数据数组，表示各次undo的量。结构定义在 include/linux/sem.h。
(2) struct sem_queue *semsleeping;
每一信号量集合对应一 个sem_queue等待队列(见include/linux/sem.h)。进程因操作该信号量集合而阻塞时，它被挂到semsleeping指示的关 于该信号量集合的sem_queue队列。反过来，semsleeping。sleeper指向该进程的PCB。
7. 进程上下文环境
(1) struct desc_struct *ldt;
进程关于CPU段式存储管理的局部描述符表的指针，用于仿真WINE Windows的程序。其他情况下取值NULL，进程的ldt就是arch/i386/traps.c定义的default_ldt。
(2) struct thread_struct tss;
任务状态段，其内容与INTEL CPU的TSS对应，如各种通用寄存器.CPU调度时，当前运行进程的TSS保存到PCB的tss，新选中进程的tss内容复制到CPU的TSS。结构定义在include/linux/tasks.h中。
(3) unsigned long saved_kernel_stack;
为MS-DOS的仿真程序(或叫系统调用vm86)保存的堆栈指针。
(4) unsigned long kernel_stack_page;
在内核态运行时，每个进程都有一个内核堆栈，其基地址就保存在kernel_stack_page中。
8. 文件系统数据成员
(1) struct fs_struct *fs;
fs 保存了进程本身与VFS的关系消息，其中root指向根目录结点，pwd指向当前目录结点，umask给出新建文件的访问模式(可由系统调用umask更 改)，count是Linux保留的属性，如下页图所示。结构定义在include/linux/sched.h中。
(2) struct files_struct *files;
files包含了进程当前所打开的文件(struct file *fd[NR_OPEN])。在Linux中，一个进程最多只能同时打开NR_OPEN个文件。而且，前三项分别预先设置为标准输入、标准输出和出错消息输出文件。
(3) int link_count;
文件链(link)的数目。
9. 内存数据成员
(1) struct mm_struct *mm;
在linux 中，采用按需分页的策略解决进程的内存需求。task_struct的数据成员mm指向关于存储管理的mm_struct结构。其中包含了一个虚存队列 mmap，指向由若干vm_area_struct描述的虚存块。同时，为了加快访问速度，mm中的mmap_avl维护了一个AVL树。在树中，所有的 vm_area_struct虚存块均由左指针指向相邻的低虚存块，右指针指向相邻的高虚存块。 结构定义在include/linux/sched.h中。
10. 页面管理
(1) int swappable:1;
进程占用的内存页面是否可换出。swappable为1表示可换出。对该标志的复位和置位均在do_fork()函数中执行(见kerenl/fork.c)。
(2) unsigned long swap_address;
虚存地址比swap_address低的进程页面，以前已经换出或已换出过，进程下一次可换出的页面自swap_address开始。参见swap_out_process()和swap_out_pmd()(见mm/vmscan.c)。
(3) unsigned long min_flt，maj_flt;
该 进程累计的minor缺页次数和major缺页次数。maj_flt基本与min_flt相同，但计数的范围比后者广(参见fs/buffer.c和 mm/page_alloc.c)。min_flt只在do_no_page()、do_wp_page()里(见mm/memory.c)计数新增的可 以写操作的页面。
(4) unsigned long nswap;
该进程累计换出的页面数。
(5) unsigned long cmin_flt，cmaj_flt，cnswap;
以本进程作为祖先的所有层次子进程的累计换入页面、换出页面计数。
(6) unsigned long old_maj_flt，dec_flt;
(7) unsigned long swap_cnt;
下一次信号最多可换出的页数。
11. 支持对称多处理器方式(SMP)时的数据成员
(1) int processor;
进程正在使用的CPU。
(2) int last_processor;
进程最后一次使用的CPU。
(3) int lock_depth;
上下文切换时系统内核锁的深度。
12. 其它数据成员
(1) unsigned short used_math;
是否使用FPU。
(2) char comm[16];
进程正在运行的可执行文件的文件名。
(3) struct rlimit rlim[RLIM_NLIMITS];
结 构rlimit用于资源管理，定义在linux/include/linux/resource.h中，成员共有两项:rlim_cur是资源的当前最大 数目;rlim_max是资源可有的最大数目。在i386环境中，受控资源共有RLIM_NLIMITS项，即10项，定义在 linux/include/asm/resource.h中。
(4) int errno;
最后一次出错的系统调用的错误号，0表示无错误。系统调用返回时，全程量也拥有该错误号。
(5) long debugreg[8];
保存INTEL CPU调试寄存器的值，在ptrace系统调用中使用。
(6) struct exec_domain *exec_domain;
Linux可以运行由80386平台其它UNIX操作系统生成的符合iBCS2标准的程序。关于此类程序与Linux程序差异的消息就由exec_domain结构保存。
(7) unsigned long personality;
Linux 可以运行由80386平台其它UNIX操作系统生成的符合iBCS2标准的程序。 Personality进一步描述进程执行的程序属于何种UNIX平台的“个性”信息。通常有PER_Linux、PER_Linux_32BIT、 PER_Linux_EM86、PER_SVR3、PER_SCOSVR3、PER_WYSEV386、PER_ISCR4、PER_BSD、 PER_XENIX和PER_MASK等，参见include/linux/personality.h。
(8) struct linux_binfmt *binfmt;
指向进程所属的全局执行文件格式结构，共有a。out、script、elf和java等四种。结构定义在include/linux/binfmts.h中(core_dump、load_shlib(fd)、load_binary、use_count)。
(9) int exit_code，exit_signal;
引起进程退出的返回代码exit_code，引起错误的信号名exit_signal。
(10) int dumpable:1;
布尔量，表示出错时是否可以进行memory dump。
(11) int did_exec:1;
按POSIX要求设计的布尔量，区分进程是正在执行老程序代码，还是在执行execve装入的新代码。
(12) int tty_old_pgrp;
进程显示终端所在的组标识。
(13) struct tty_struct *tty;
指向进程所在的显示终端的信息。如果进程不需要显示终端，如0号进程，则该指针为空。结构定义在include/linux/tty.h中。
(14) struct wait_queue *wait_chldexit;
在进程结束时，或发出系统调用wait4后，为了等待子进程的结束，而将自己(父进程)睡眠在该队列上。结构定义在include/linux/wait.h中。
13. 进程队列的全局变量
(1) current;
当前正在运行的进程的指针，在SMP中则指向CPU组中正被调度的CPU的当前进程:
#define current(0+current_set[smp_processor_id()])/*sched.h*/
struct task_struct *current_set[NR_CPUS];
(2) struct task_struct init_task;
即0号进程的PCB，是进程的“根”，始终保持初值INIT_TASK。
(3) struct task_struct *task[NR_TASKS];
进 程队列数组，规定系统可同时运行的最大进程数(见kernel/sched.c)。NR_TASKS定义在include/linux/tasks.h 中，值为512。每个进程占一个数组元素(元素的下标不一定就是进程的pid)，task[0]必须指向init_task(0号进程)。可以通过 task[]数组遍历所有进程的PCB。但Linux也提供一个宏定义for_each_task()(见 include/linux/sched.h)，它通过next_task遍历所有进程的PCB:
#define for_each_task(p) \
for(p=&init_task;(p=p->next_task)!=&init_task;)
(4) unsigned long volatile jiffies;
Linux的基准时间(见kernal/sched.c)。系统初始化时清0，以后每隔10ms由时钟中断服务程序do_timer()增1。
(5) int need_resched;
重新调度标志位(见kernal/sched.c)。当需要Linux调度时置位。在系统调用返回前(或者其它情形下)，判断该标志是否置位。置位的话，马上调用schedule进行CPU调度。
(6) unsigned long intr_count;
记 录中断服务程序的嵌套层数(见kernel/softirq.c)。正常运行时，intr_count为0。当处理硬件中断、执行任务队列中的任务或者执 行bottom half队列中的任务时，intr_count非0。这时，内核禁止某些操作，例如不允许重新调度。