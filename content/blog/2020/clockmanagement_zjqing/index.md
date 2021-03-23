---
title: "Linux时间管理"
date: 2020-09-01T16:50:24+08:00
author: "张纪庆"
keywords: ["ticks",“jiffies”]
categories : ["时钟管理"]
banner : "img/blogimg/ljrimg19.jpg"
summary : "时间管理在内核中占有非常重要的地位，内核中有大量的函数都是基于时间驱动的，比如调度程序中的运行队列进行平衡调整、对屏幕进行刷新等，需要周期执行的函数；再比如需要等待一个相对时间再运行的任务。除上述情况外需要内核提供时间外，内核还必须管理系统的运行时间以及当前时间和日期。"

---

# 前言

​		时间管理在内核中占有非常重要的地位，内核中有大量的函数都是基于时间驱动的，比如调度程序中的运行队列进行平衡调整、对屏幕进行刷新等，需要周期执行的函数；再比如需要等待一个相对时间再运行的任务。除上述情况外需要内核提供时间外，内核还必须管理系统的运行时间以及当前时间和日期。



# Linux时间管理的基本概念

​		时间概念对计算机来说有些模糊，内核必须在硬件的帮助下才能计算和管理时间，硬件为内核提供了一个系统定时器用来计算流逝的时间。

## 硬件设备

​		系统提供了两种设计进行计时：系统定时器和实时时钟

- ##### 实时时钟

  实时时钟RTC用来持久存放系统时间的设备，系统关闭后，也可以靠主板上的微型电池提供的电力保持系统的计时，在PC体系结构中，RTC和CMOS集成在一起，并且RTC的运行和BIOS的保存设置都是通过同一个电池供电的

- [ ] RTC存放在xtime中，系统启动后，内核通过读取RTC来初始化墙上时间；当完成该读取后，内核一般不会在从xtime中读取数据，但在有些体系结构中，比如x86会周期性地将当前时间值存回到RTC中。

- ##### 系统定时器

  其思想是提供一种周期性触发中断机制，有些体系结构是通过电子晶振进行分频来实现系统定时器，还有一些体系解耦股提供了一个衰减测量器decrementer--为其设置一个初始值，该值以固定频率递减，当减到0的时候，触发一个中断。

  比如，在x86中采用可编程中断时钟PIT，内核在启动时对PIT进行编程初始化，使其能够以Hz/秒的频率产生时钟中断。

## 时间概念

### 节拍ticks

​		ticks是系统时钟中断的间隔，该值与节拍率Hz有关，ticks=1/Hz。

​       系统定时器以某种频率自行触发时钟中断，该频率可以通过编程预定，称作节拍率（tick rate)。节拍率有一个Hz频率，一个节拍时长为1/Hz。

​       不同的体系结构，节拍率的值可能不同。而且节拍率的值是可以改变的，其定义在<param.h>中：

```c
#ifndef _UAPI__ASM_GENERIC_PARAM_H
#define _UAPI__ASM_GENERIC_PARAM_H
#ifndef HZ
#define HZ 100
#endif
#ifndef EXEC_PAGESIZE
#define EXEC_PAGESIZE	4096
#endif
#ifndef NOGROUP
#define NOGROUP		(-1)
#endif
#define MAXHOSTNAMELEN	64	/* max length of hostname */
#endif /* _UAPI__ASM_GENERIC_PARAM_H */

/*
在shell中可以使用命名查看当前系统的Hz值：
root@zjq-virtual-machine:/home/zjq# uname -a
Linux zjq-virtual-machine 5.4.0-42-generic #46-Ubuntu SMP Fri Jul 10 00:24:02 UTC 2020 x86_64 x86_64 x86_64 GNU/Linux
root@zjq-virtual-machine:/home/zjq# cat /boot/config-5.4.0-42-generic | grep CONFIG_HZ
# CONFIG_HZ_PERIODIC is not set
# CONFIG_HZ_100 is not set
CONFIG_HZ_250=y
# CONFIG_HZ_300 is not set
# CONFIG_HZ_1000 is not set
CONFIG_HZ=250
从这里我们可以看得出来，该机器的Hz为250，那么ticks=4ms
*/
```

​        随着硬件和体系结构的发展，节拍率越来越高，ticks越来越短，那么带来了那些好处和弊端那？

​       提高节拍率（时钟频率）意味着时钟中断产生得更加频繁，中断处理程序也会更频繁地执行，由此带来的好处有：更高的时钟中断解析度，可提高时间驱动时间的解析度；提高了时间驱动时间的准确度。由此带来了一些优点，比如：

- 内核定时器能够以更高的频率表和更高的准确度运行；
- 依赖定时值执行的系统调用，比如poll()和select()能够以更高的精度运行；
- 对诸如资源消耗和系统运行时间等的测量会有更精细的解析度；
- 提高进程抢占的准确度。

​        但是由此也带来了一些劣势，比如：节拍率越高，时钟中断频率越高，也就意味着系统负担越重，因为处理器需花时间来执行时钟中断处理程序，节拍率提高，中断处理程序占用的处理器的时间越来越多。

### 全局变量jiffies

​        全局变量jiffies用来记录子系统启动以来产生的节拍的总数，每次执行时钟中断处理程序就会增加1，所以1秒内增加的数量为Hz。

​		实际应用复杂一些：内核给jiffies附一个特殊的初值，引起这个变量不断地溢出，由此捕捉Bug，当找到实际jiffies之后，就首先把这个偏差减去。

```C
/*
jiffies定义域文件<linux/jiffies.h>中：jiffies是无符号长整型
*/
	extern u64 __cacheline_aligned_in_smp jiffies_64;
	extern unsigned long volatile __cacheline_aligned_in_smp __jiffy_arch_data jiffies;
/*
ld（1）脚本用于连接主内核映像，然后用jiffies_64变量的初值覆盖jiffies变量：
	jiffies=jiffies_64
在32位系统上jiffies这样就只取jiffies_64的低32位
在64位系统上两者相等
通过get_jiffies_64()函数获得jiffies_64
*/
```

![](img\jiffies_64.png)

​		由jiffies的定义可知，其具有最大值。那么当jiffies超过长整型能够存放的最大值时，就会回绕到0。

​		但回绕到0之后又出现了新的问题，在如下所示的代码中：

![](img\Wrap_around_to_zore.png)



​		如果设置了timeout之后，jiffies回绕了，那么if就会返回True，正好与实际效果相反。为了能够正确处理节拍回绕问题，在<linux/jiffies>中进行了类似的定义：

```c
#define time_after(a,b)		\
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)((b) - (a)) < 0))
#define time_before(a,b)	time_after(b,a)

#define time_after_eq(a,b)	\
	(typecheck(unsigned long, a) && \
	 typecheck(unsigned long, b) && \
	 ((long)((a) - (b)) >= 0))
#define time_before_eq(a,b)	time_after_eq(b,a)

//a参数通常是jiffies，b参数是需要对比的值
```

那么上述代码就可以改成如下版本：

![](img\Wrap_around_to_zore_update.png)

# 时钟中断处理程序

​		时钟中断对于管理操作系统尤为重要，大量内核函数的生命周期都离不开流逝的时间的控制，那么在时钟中断周期内执行那些任务哪？

- 更新系统运行时间；
- 更新实际时间；
- 在smp系统上，均衡调度程序中各处理器上的运行队列；
- 检查当前进程是否用尽了自己的时间片，如果用尽，就重新调度；
- 运行超时的动态定时器；
- 更新资源消耗和处理器时间的统计值。

​        上述的工作有些随时钟的频率反复执行，另外一些也是周期性地执行，但是需要n次时钟中断运行一次。也就是说，这些函数在累计了一定数量的时钟节拍时才被执行。

​        在时钟中断时，会启动时钟中断处理程序来处理中断，时钟中断处理程序分为两部分：

- 体系结构无关部分
- 体系结构相关部分：作为系统定时器的中断处理程序而注册到内核中，当产生时钟中断时，能够相应的运行，执行的工作有：
  - 获得xtime_lock锁，以便对访问jiffies_64和墙上时间xtime进行保护
  - 需要时应答或重新设置系统时钟
  - 周期性地使用墙上时间更新实时时钟
  - 调用体系结构无关的时钟例程：tick_periodic()，剩下的工作由它完成：
    - 给jiffies_64变量增加1，此时的操作是安全的因为，前面已经获得了xtime_lock锁
    - 更新资源消耗的统计值，比如当前进程所消耗的系统时间和用户时间
    - 执行已经到期的动态定时器
    - 执行schedule_tick()函数
    - 计算平均负载值

```c
static void tick_periodic(int cpu)
{
	if (tick_do_timer_cpu == cpu) {
		raw_spin_lock(&jiffies_lock);
		write_seqcount_begin(&jiffies_seq);

		/* Keep track of the next tick event */
		tick_next_period = ktime_add(tick_next_period, tick_period);

		do_timer(1);
		write_seqcount_end(&jiffies_seq);
		raw_spin_unlock(&jiffies_lock);
		update_wall_time();  //更新墙上时钟
	}

	update_process_times(user_mode(get_irq_regs()));
	profile_tick(CPU_PROFILING);
}

//主要工作是由以下两个函数完成的：
//do_timer()函数：
void do_timer(unsigned long ticks)
{
	jiffies_64 += ticks;
	calc_global_load(ticks);  //更新系统的平均负载统计值
}

//当do_timer()最终返回时，调用update_process_times()更新所耗费的各种节拍数：
void update_process_times(int user_tick)
{
	struct task_struct *p = current;

	/* Note: this timer irq context must be accounted for as well. */
	account_process_tick(p, user_tick);   //详解如下所示
	run_local_timers();             //标记了一个软中断去处理所有到期的定时器
	rcu_sched_clock_irq(user_tick);
#ifdef CONFIG_IRQ_WORK
	if (in_irq())
		irq_work_tick();
#endif
	scheduler_tick();    //负责减少当前运行进程的时间片计数值并且在需要时设置need_resched标志
	if (IS_ENABLED(CONFIG_POSIX_TIMERS))
		run_posix_cpu_timers();

	/* The current CPU might make use of net randoms without receiving IRQs
	 * to renew them often enough. Let's update the net_rand_state from a
	 * non-constant value that's not affine to the number of calls to make
	 * sure it's updated when there's some activity (we don't care in idle).
	 */
	this_cpu_add(net_rand_state.s1, rol32(jiffies, 24) + user_tick);
}
//user_ticks区别是花费在用户空间还是内核空间，其值是通过查看系统寄存器来设置的：具体代码见上方


//account_process_tick()函数对进程时间进行实质性更新：
void account_process_tick(struct task_struct *p, int user_tick)
{
	u64 cputime, steal;

	if (vtime_accounting_enabled_this_cpu())
		return;

	if (sched_clock_irqtime) {
		irqtime_account_process_tick(p, user_tick, 1);
		return;
	}

	cputime = TICK_NSEC;
	steal = steal_account_process_time(ULONG_MAX);

	if (steal >= cputime)
		return;

	cputime -= steal;

	if (user_tick)
		account_user_time(p, cputime);
	else if ((p != this_rq()->idle) || (irq_count() != HARDIRQ_OFFSET))
		account_system_time(p, HARDIRQ_OFFSET, cputime);
	else
		account_idle_time(cputime);
}
/*
根据上述代码可以得出结论，内核对进程时间计数时，是根据：
    中断发生时处理器所处的模式进行统计的，它把上一个节拍的都算给了进程，但是实际情况是进程在上一个节拍期间可能多次进入和退出内核模式，而且在上一个节拍期间 该进程也不一定是唯一一个运行进程。
*/
```

​		tick_periodic()函数执行完毕后返回与体系结构相关的中断处理程序，继续执行后面的工作，释放xtime_lock锁，然后退出。tick_periodic()执行流程如下图所示：

<img src="img\tick_periodic.JPG" style="zoom:80%;" />

# 定时器

​		有一些工作是需要在指定时间点执行，这时候就需要用到定时器，我们只需要执行一些初始化工作，设置一个超时时间，指定超时发生后执行的函数，然后激活定时器就可以了。当定时器到期后，就会自动执行执行的函数。

​		定时器结构time_list定义在<linux/timer.h>中：

```c
struct timer_list {
	/*
	 * All fields that change during normal runtime grouped to the
	 * same cacheline
	 */
	struct list_head entry;
	unsigned long expires;
	struct tvec_base *base;

	void (*function)(unsigned long);
	unsigned long data;

	int slack;

#ifdef CONFIG_TIMER_STATS
	int start_pid;
	void *start_site;
	char start_comm[16];
#endif
#ifdef CONFIG_LOCKDEP
	struct lockdep_map lockdep_map;
#endif
};
```

​		当我们使用定时器时，不要过多陷入到这个结构的定义中，内核提供了一组与定时器相关的接口用来简化管理定时器的操作，这些接口的声明在文件<linux/timer.h>中，大多数接口在文件kernel/timer.c中获得：

```c
/*
1.创建一个定时器：
struct timer_list my_timer；
通过辅助函数来初始化定时器数据结构的内部值，初始化必须在使用其他定时器管理函数对定时器操作前完成：
init_timer(&my_timer);

2.填充结构中需要的值：
my_timer.expries=jiffies+delay;  //定时器超时的节拍数
my_timer.data=0;                 //给定时器处理函数传入0值
my_timer.function=my_function；  //定时器超时时调用的函数
    my_function函数的定义如下：
    void my_timer_function(unsigned long data);
data参数使你可以利用同一个处理函数注册多个定时器，只需通过该函数就能区别对待他们
如果不需要这个参数，就可以简单地传递0给处理函数
    
3.最后激活定时器：
add_timer（&my_timer）；
    
4.可以更改已经激活的定时器超时时间，使用mod_timer()：
mod_timer(&my_timer,jiffies+new_delay);  //新的定时器

5.停止未超时的定时器del_timer()：
del_timer(&my_timer);

注意：del_timer()只是保证定时器不会被激活，但是在多处理器上定时器中断可能已经在其他处理器上运行了，所以删除定时器时需要等待可能在其他处理器上运行的定时处理程序都退出，这时需要del_timer_sync()函数执行删除工作：
del_timer_sync(&my_timer);
和del_timer()不同，它不可以在中断上下文中使用
*/
```

​		那么我们的定时器是如何实现的那？

```c
/*
具体实现：
时钟中断处理程序会执行update_process_times()函数，
该函数随即调用run_local_timers()函数： */
void run_local_timers(void){
    hrtimer_run_queues();
    raise_softirq(TIMER_SOFTIRQ);   //执行定时器软中断
    softlockup_tick();
}

//run_timer_softirq()函数处理软中断TIMER_SOFTIRQ，从而在当前处理器上运行所有的（如果还有的话）超时定时器
static void run_timer_softirq(struct softirq_action *h)
{
	struct tvec_base *base = __this_cpu_read(tvec_bases);

	hrtimer_run_pending();

	if (time_after_eq(jiffies, base->timer_jiffies))
		__run_timers(base);
}
```

​		注意：所有的定时器都以链表的形式存放在一起，但是让内核经常为了寻找超时定时器而遍历整个链表是不明智对的，按照时间排序也是不明智的，所以内核将他们按照超时时间划分为5个组，当定时器超时时间接近时，定时器将随组一起下移。

# 延迟执行

​		内核代码处理使用定时器或下半部机制外，还需要其他方法来推迟执行任务，这种推迟通常发生在等待应完成某些工作时，而且等待的时间往往非常短。

​		实现延迟执行有两种思路：在延迟任务时挂起处理器，防止处理器执行任何实际工作；不挂起处理器，但不能保证被延迟的代码能够在指定的延迟时间处理。具体方式如下：

- ​	忙等待：延迟的时间是节拍的整数倍

```c
/*
实现：在循环中不断旋转直到希望的时钟节拍数耗尽
unsigned long timeout=jiffies+10；  //10个节拍
while（time_before(jiffies,timeout));

或者直到jiffies大于delay为止：
unsigned long delay = jiffies+ 2*Hz;  //2秒
while(time_before((jiffies,delay))；

上述方法并不是最优的，因为这是的CPU也在空转，更好的方法是在代码等待的时候，
允许重新调度执行其他任务：
unsigned long delay=jiffies + 5*Hz;
while(time_before(jiffies,delay))
    cond_resched();
cond_resched()函数将一个新程序投入运行，但它只有在设置完need_resched标志后才能生效。
注意：因为该方法需要调用调度程序，所以它不能在中断上下文中使用，只能在进程上下文使用。
有一个重要的问题：
C编译器只将变量装载一次，一般情况下不能保证循环中的jiffies变量在每次循环中被读取时都重新被装载，但是我们要求jiffies在每次循环是都必须能重新装载，因为在后台jiffies值
会随时钟中断的发生而不断增加。
为了解决这个问题，<linux/jiffies.h>中jiffies变量被标记为关键字volatile，它指示编译器每次访问变量时都重新从内存中获得，而不是通过寄存器中的变量别名来访问，从而确保
前面的循环能按预期的方式执行
*/
```

- 短延迟：有些内核代码需要的延迟可能比时钟节拍还短，并且要求延迟的时间很精确，比如和硬件同步时，此时内核提供了三个可以处理ms，ns，us级别的延迟函数：

```c
void udelay(unsigned long usecs);
void ndelay(unsidned long nsecs);
void mdelay(unsigned long nsecs);
/*
例如：
udelay(150);   //延迟150us
udelay()函数依靠执行数次循环达到延迟效果，而mdelay()函数又是通过udelay()函数实现的，因为内核知道处理器在1秒内执行多少次循环，所以udelay()函数仅仅需要根据指定的延迟时间，在1秒中占的比例，就能决定需要进行多少次循环就能达到要求的延迟时间。
那么内核如何知道循环多少次的那？
答：通过Bogomips，它是Linux操作系统中衡量计算机处理器运行速度的的一种尺度，是由Linux主要开发者linus Torvalds写的。该值存放在loops_per_jiffy中，可以从文件/proc/cpuinfo中读到它。 
    注意：它是通过calibrate_delay（）函数计算出来的。只能用来粗略计算处理器的性能，并不十分精确。
    */
```

- schedule_timeout()函数：让需要延迟执行的任务睡眠到指定的延迟时间耗尽后在重新执行。但是该方法不能保证睡眠时间正好等于指定的延迟时间，只能尽量是睡眠时间接近指定的延迟时间：

```c
set_current_state(TASK_INTERRUPTIBLE/TASK_UNINTERRUPTIBLE);
schedule_timeout(s*Hz);
//注意：必须设置为上述两种状态，否则进程无法休眠

//1.schedule_timeout的实现：
signed long __sched schedule_timeout(signed long timeout)
{
	struct timer_list timer;
	unsigned long expire;

	switch (timeout)
	{
	case MAX_SCHEDULE_TIMEOUT:
		/*
		 * These two special cases are useful to be comfortable
		 * in the caller. Nothing more. We could take
		 * MAX_SCHEDULE_TIMEOUT from one of the negative value
		 * but I' d like to return a valid offset (>=0) to allow
		 * the caller to do everything it want with the retval.
		 */
		schedule();
		goto out;
	default:
		/*
		 * Another bit of PARANOID. Note that the retval will be
		 * 0 since no piece of kernel is supposed to do a check
		 * for a negative retval of schedule_timeout() (since it
		 * should never happens anyway). You just have the printk()
		 * that will tell you if something is gone wrong and where.
		 */
		if (timeout < 0) {
			printk(KERN_ERR "schedule_timeout: wrong timeout "
				"value %lx\n", timeout);
			dump_stack();
			current->state = TASK_RUNNING;
			goto out;
		}
	}

	expire = timeout + jiffies;

	setup_timer_on_stack(&timer, process_timeout, (unsigned long)current);
	__mod_timer(&timer, expire, false, TIMER_NOT_PINNED);
	schedule();
	del_singleshot_timer_sync(&timer);

	/* Remove the timer from the object tracker */
	destroy_timer_on_stack(&timer);

	timeout = expire - jiffies;

 out:
	return timeout < 0 ? 0 : timeout;
}


//定时器超时后，会调用process_timeout()，该函数将任务设置为TASK_RUNNING状态，然后将其放入运行队列：
static void process_timeout(unsigned long __data)
{
	wake_up_process((struct task_struct *)__data);
}

//2.设置超时时间，在等待队列上睡眠：
//等待队列上的某个任务既可能在等在一个特定事件到来，又在等待一个特定时间到期 就看谁先到来
```

# 总结

​		时间管理是系统正常运行的基础，在进程调度中多个进程间共享CPU资源就是通过分时实现，并且对于监控系统性能也是具有重要意义的，ticks的大小对于性能监控的准确性是具有重要意义的。