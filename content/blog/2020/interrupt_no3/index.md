---
title: "从RTC设备学习中断"
date: 2020-07-05T12:09:13+08:00
author: "编辑：张孝家"
keywords: ["中断"]
categories : ["电子杂志"]
banner : "img/blogimg/zxj0.jpg"
summary : "本章将向读者依次解释中断概念，解析 Linux 中的中断实现机理以及 Linux 下中断如何被使用。作为实例我们将向第二期中打造的系统中加入一个时钟中断，希望可以帮助读者掌握中断相关的概念和编程方法。"
---

# 系统实时钟
每台PC机都有一个实时钟（Real Time Clock）设备。在你关闭计算机电源的时候，由它维持系统的日期和时间信息。

此外，它还可以用来产生周期信号，频率变化范围从2Hz到8192Hz——当然，频率必须是2的倍数。这样该设备就能被当作一个定时器使用，比如我们把频率设定为4Hz，那么设备启动后，系统实时钟每秒就会向CPU发送4次定时信号——通过8号中断提交给系统（标准PC机的IRQ 8是如此设定的）。由于系统实时钟是可编程控制的，你也可以把它设成一个警报器，在某个特定的时刻拉响警报——向系统发送IRQ 8中断信号。由此看来，IRQ 8与生活中的闹铃差不多：中断信号代表着报警器或定时器的发作。

在Linux操作系统的实现里，上述中断信号可以通过/dev/rtc（主设备号10，从设备号135，只读字符设备）设备获得。对该设备执行读（read）操作，会得到unsigned long型的返回值，最低的一个字节表明中断的类型（更新完毕update-done，定时到达alarm-rang，周期信号periodic）；其余字节包含上次读操作以来中断到来的次数。如果系统支持/proc文件系统，/proc/driver/rtc中也能反映相同的状态信息。

该设备只能由每个进程独占，也就是说，在一个进程打开(open)设备后，在它没有释放前，不允许其它进程再打开它。这样，用户的程序就可以通过对/dev/rtc执行read()或select()系统调用来监控这个中断——用户进程会被阻塞，直到系统接收到下一个中断信号。对于一些高速数据采集程序来说，这个功能非常有用，程序无需死守着反复查询，耗尽所有的CPU资源；只要做好设定，以一定频率进行查询就可以了。注2

```c
#include <stdio.h>
#include <linux/rtc.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

int main(void)
{
  int i, fd, retval, irqcount = 0;
  unsigned long tmp, data;
  struct rtc_time rtc_tm;

  // 打开RTC设备
  fd = open ("/dev/rtc", O_RDONLY);

  if (fd ==  -1) {
    perror("/dev/rtc");
    exit(errno);
  }
  
  fprintf(stderr, "\n\t\t\tEnjoy TV while boiling water.\n\n");
  
  // 首先是一个报警器的例子，设定10分钟后"响铃"
  // 获取RTC中保存的当前日期时间信息
  /* Read the RTC time/date */
  retval = ioctl(fd, RTC_RD_TIME, &rtc_tm);
  if (retval == -1) {
    perror("ioctl");
    exit(errno);
  }

  fprintf(stderr, "\n\nCurrent RTC date/time is %d-%d-%d,%02d:
%02d:%02d.\n", 
    rtc_tm.tm_mday, rtc_tm.tm_mon + 1, rtc_tm.tm_year + 1900,
     rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);

  // 设定时间的时候要避免溢出
  rtc_tm.tm_min += 10;
  if (rtc_tm.tm_sec >= 60) {
    rtc_tm.tm_sec %= 60;
    rtc_tm.tm_min++;
  }
  if  (rtc_tm.tm_min == 60) {
    rtc_tm.tm_min = 0;
    rtc_tm.tm_hour++;
  }
  if  (rtc_tm.tm_hour == 24)
    rtc_tm.tm_hour = 0;

  // 实际的设定工作
  retval = ioctl(fd, RTC_ALM_SET, &rtc_tm);
  if (retval == -1) {
    perror("ioctl");
    exit(errno);
  }

  // 检查一下，看看是否设定成功
  /* Read the current alarm settings */
  retval = ioctl(fd, RTC_ALM_READ, &rtc_tm);
  if (retval == -1) {
    perror("ioctl");
    exit(errno);
  }

  fprintf(stderr, "Alarm time now set to %02d:%02d:%02d.\n",
     rtc_tm.tm_hour, rtc_tm.tm_min, rtc_tm.tm_sec);

  // 光设定还不成，还要启用alarm类型的中断才行
  /* Enable alarm interrupts */
  retval = ioctl(fd, RTC_AIE_ON, 0);
  if (retval == -1) {
    perror("ioctl");
    exit(errno);
  }
  
  // 现在程序可以耐心的休眠了，10分钟后中断到来的时候它就会被唤醒
  /* This blocks until the alarm ring causes an interrupt */
  retval = read(fd, &data, sizeof(unsigned long));
  if (retval == -1) {
    perror("read");
    exit(errno);
  }
  irqcount++;
  fprintf(stderr, " okay. Alarm rang.\n");
}
```
这个例子稍微显得有点复杂，用到了open、ioctl、read等诸多系统调用，初看起来让人眼花缭乱。其实如果简化一下的话，过程还是“烧开水”：设定定时器、等待定时器超时、执行相应的操作（“关煤气灶”）。

读者可能不理解的是：这个例子完全没有表现出中断带来的好处啊，在等待10分钟的超时过程中，程序依然什么都不能做，只能休眠啊？

读者需要注意自己的视角，我们所说的中断能够提升并发处理能力，提升的是CPU的并发处理能力。在这里，上面的程序可以被看作是烧开水，在烧开水前，闹铃已经被上好，10分钟后CPU会被中断（闹铃声）惊动，过来执行后续的关煤气工作。也就是说，CPU才是这里唯一具有处理能力的主体，我们在程序中主动利用中断机制来节省CPU的耗费，提高CPU的并发处理能力。这有什么好处呢？试想如果我们还需要CPU烤面包，CPU就有能力完成相应的工作，其它的工作也一样。这其实是在多任务操作系统环境下程序生存的道德基础——“我为人人，人人为我”。

好了，这段程序其实是我们进入Linux中断机制的引子，现在我们就进入Linux中断世界。

注2：更详细的内容和其它一些注意事项请参考内核源代码包中Documentations/rtc.txt

# RTC中断服务程序
RTC中断服务程序包含在内核源代码树根目录下的driver/char/rtc.c文件中，该文件正是RTC设备的驱动程序——我们曾经提到过，中断服务程序一般由设备驱动程序提供，实现设备中断特有的操作。

SagaLinux中注册中断的步骤在Linux中同样不能少，实际上，两者的原理区别不大，只是Linux由于要解决大量的实际问题（比如SMP的支持、中断的共享等）而采用了更复杂的实现方法。

RTC驱动程序装载时，rtc_init()函数会被调用，对这个驱动程序进行初始化。该函数的一个重要职责就是注册中断处理程序：

```c
if (request_irq(RTC_IRQ,rtc_interrupt,SA_INTERRUPT,”rtc”,NULL)){
	printk(KERN_ERR “rtc:cannot register IRQ %d\n”,rtc_irq);
	return –EIO;
}
```
这个request_irq函数显然要比SagaLinux中同名函数复杂很多，光看看参数的个数就知道了。不过头两个参数两者却没有区别，依稀可以推断出：它们的主要功能都是完成中断号与中断服务程序的绑定。

关于Linux提供给系统程序员的、与中断相关的函数，很多书籍都给出了详细描述，如“Linux Kernel Development”。我这里就不做重复劳动了，现在集中注意力在中断服务程序本身上。

```c
static irqreturn_t rtc_interrupt(int irq, void *dev_id,
struct pt_regs *regs)
{
        /*
         *     Can be an alarm interrupt, update complete interrupt,
         *     or a periodic interrupt. We store the status in the
         *     low byte and the number of interrupts received since
         *     the last read in the remainder of rtc_irq_data.
         */
        spin_lock (&rtc_lock);
        rtc_irq_data += 0x100;
        rtc_irq_data &= ~0xff;
        rtc_irq_data |= (CMOS_READ(RTC_INTR_FLAGS) & 0xF0);

        if (rtc_status & RTC_TIMER_ON)
                mod_timer(&rtc_irq_timer,
jiffies + HZ/rtc_freq
 + 2*HZ/100);
        spin_unlock (&rtc_lock);

        /* Now do the rest of the actions */
        spin_lock(&rtc_task_lock);
        if (rtc_callback)
                rtc_callback->func(rtc_callback->private_data);
        spin_unlock(&rtc_task_lock);
        wake_up_interruptible(&rtc_wait);      
        kill_fasync (&rtc_async_queue, SIGIO, POLL_IN);
        return IRQ_HANDLED;
}
```

这里先提醒读者注意一个细节：中断服务程序是static类型的，也就是说，该函数是本地函数，只能在rtc.c文件中调用。这怎么可能呢？根据我们从SagaLinux中得出的经验，中断到来的时候，操作系统的中断核心代码一定会调用此函数的，否则该函数还有什么意义？实际上，request_irq函数会把指向该函数的指针注册到相应的查找表格中（还记得SagaLinux中的irq_handler[]吗？）。static只能保证rtc.c文件以外的代码不能通过函数名字显式地调用函数，而对于指针，它就无法画地为牢了。

程序用到了spin_lock函数，它是Linux提供的自旋锁相关函数，关于自旋锁的详细情况，我们会在以后的文章中详细介绍。你先记住，自旋锁是用来防止SMP结构中的其他CPU并发访问数据的，在这里被保护的数据就是rtc_irq_data。rtc_irq_data存放有关RTC的信息，每次中断时都会更新以反映中断的状态。

接下来，如果设置了RTC周期性定时器，就要通过函数mod_timer()对其更新。定时器是Linux操作系统中非常重要的概念，我们会在以后的文章中详加解释。

代码的最后一部分要通过设置自旋锁进行保护，它会执行一个可能被预先设置好的回调函数。RTC驱动程序允许注册一个回调函数，并在每个RTC中断到来时执行。

wake_up_interruptible是个非常重要的调用，在它执行后，系统会唤醒睡眠的进程，它们等待的RTC中断到来了。这部分内容涉及等待队列，我们也会在以后的文章中详加解释。

# 最简单的改动
我们来更进一步感受中断，非常简单，我们要在RTC的中断服务程序中加入一条printk语句，打印什么呢？“I’m coming, interrupt!”。

下面，我们把它加进去：

```c
… …
spin_unlock(&rtc_task_lock);
printk(“I’m coming , interrupt!\n”);
wake_up_interruptible(&rtc_wait);    
… …
```

没错，就先做这些，请你找到代码树的drivers\char\rtc.c文件，在其中irqreturn_t rtc_interrupt函数中加入这条printk语句。然后重新编译内核模块（当然，你要在配置内核编译选项时包含RTC，并且以模块形式）现在，当我们插入编译好的rtc.o模块，执行前面实时钟部分介绍的用户空间程序，你就会看到屏幕上打印的“I’m coming , interrupt!”信息了。

这是一次实实在在的中断服务过程，如果我们通过ioctl改变RTC设备的运行方式，设置周期性到来的中断的话，假设我们将频率定位8HZ，你就会发现屏幕上每秒打印8次该信息。

动手修改RTC实际上是对中断理解最直观的一种办法，我建议你不但注意中断服务程序，还可以看一下RTC驱动中ioctl的实现，这样你会更加了解外部设备和驱动程序、中断服务程序之间实际的互动情况。

不仅如此，通过修改RTC驱动程序，我完成了不少稀奇古怪的工作，比如说，在高速数据采集过程中，我就是利用高频率的RTC中断检查高速AD采样板硬件缓冲区使用情况，配合DMA共同完成数据采集工作的。当然，在有非常严格时限要求的情况下，这样不一定适用。但是，在两块12位20兆采样率的AD卡交替工作，对每秒1KHz的雷达视频数据连续采样的情况下，我的RTC跑得相当好。

当然，这可能不是一种美观和标准的做法，但是，我只是一名程序员而不是艺术家，只是了解了这么一点点中断知识，我就完成了工作，我想或许您也希望从系统底层的秘密中获得收益吧，让我们在以后的文章中再见。