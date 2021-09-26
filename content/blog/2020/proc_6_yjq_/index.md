---
title: "proc文件系统探索 之 proc根目录下的文件[六]"
date: 2020-12-26T11:00:15+08:00
author: "陈继峰 杨骏青转"
keywords: ["proc"]
categories : ["走进内核"]
banner : "img/blogimg/cxl_4.jpg"
summary : " /proc/stat文件包含了系统启动后的一些系统统计信息。"
---

 **/proc/stat** 文件包含了系统启动后的一些系统统计信息。

```
`Cat /proc/stat: cpu 77781 1077 7602 390390 13232 216 100 0 0 cpu0 77781 1077 7602 390390 13232 216 100 0 0 intr 401502 313 2041 0 2 1 0 0 0 3 0 0 0 323410 0 50372 0 0 0 0 0 0 24632 728 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 0 ctxt 4495297 btime 1225100748 processes 6295 procs_running 2 procs_blocked 0`
```

上面是我机子上的文件内容。 第一行的CPU是你所有CPU负载信息的总和。后面的CPUn（n是数字）表示你那个CPU的信息，我这里只有一个CPU，所以是CPU0。也就是说我的前两行信息相同。 其中的每一个数字代表一个相关信息，可以用如下数据结构表示：

```
`struct cpu_info { U64 user; U64 system; U64 nice; U64 idle; U64 iowait; U64 irq; U64 softirq; };`
```

user 

： 从系统启动开始累计到当前时刻，用户态的CPU时间（单位：jiffies） ，不包含 nice值为负进程。1jiffies=0.01秒 

nice

： 从系统启动开始累计到当前时刻，nice值为负的进程所占用的CPU时间（单位：jiffies） 

system

 ： 从系统启动开始累计到当前时刻，核心时间（单位：jiffies） 

idle

 ： 从系统启动开始累计到当前时刻，除硬盘IO等待时间以外其它等待时间（单位：jiffies） 

iowait

 ： 从系统启动开始累计到当前时刻，硬盘IO等待时间（单位：jiffies） ， 

irq

： 从系统启动开始累计到当前时刻，硬中断时间（单位：jiffies） 

softirq 

： 从系统启动开始累计到当前时刻，软中断时间（单位：jiffies） 从2.6.11加了第8列

stealstolen time

：which is the time spent in other operating systems when running in a virtualized environment 从 2.6.24加了第9列 

guest

： which is the time spent running a virtual  CPU  for  guest operating systems under the control of the Linux kernel. 

intr

:这行给出中断的信息，第一个为自系统启动以来，发生的所有的中断的次数；然后每个数对应一个特定的中断自系统启动以来所发生的次数，依次对应的是0号中断发生的次数，1号中断发生的次数...... 

ctxt

:给出了自系统启动以来CPU发生的上下文交换的次数 

btime

:给出了从系统启动到现在为止的时间，单位为秒 

processes

:自系统启动以来所创建的任务的个数目 

procs_running

:当前运行队列的任务的数目 

procs_blocked

:当前被阻塞的任务的数目 可以从这个文件提取一些数据来计算处理器的使用率。 处理器使用率 ： 从/proc/stat中提取四个数据：用户模式（user）、低优先级的用户模式（nice）、内核模式（system）以及空闲的处理器时间（idle）。它们均位于/proc/stat文件的第一行。CPU的利用率使用如下公式来计算。 CPU利用率   =   100   *（user   +   nice   +   system）/（user   +   nice   +   system   +   idle） 

/proc/uptime





2个数字的意义，第一个数值代表系统总的启动时间，第二个数值则代表系统空闲的时间，都是用秒来表示的。 由上面的显示来计算，开机1.698469444小时， 0.829277667%的时间都是空闲的。 

**/proc/swaps**

 这个文件显示的是交换分区的使用情况。例如我的机子上交换分区的情况如下：

```
`cjf@xiyoulinux-desktop:/proc$ cat swaps Filename                Type        Size     Used     Priority /dev/sda2              partition 996020     37260    -1`
```

看下面的英文解释： /proc/swaps provides a snapshot of every swap file name（我的是/dev/sda2）, the type of swap space(类型是：partition), the total size（我的交换分区总大小996020KB）, and the amount of space in use (in kilobytes)（使用了37260KB）. The priority column is useful when multiple swap files are in use. The lower the priority, the more likely the swap file is to be used. 如果有多个交换分区，如果它的优先级（priority）越小，越容易被使用。