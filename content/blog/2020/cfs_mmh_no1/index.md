---
title: "CFS 调度器"
date: 2020-06-06T18:13:27+08:00
author: "马明慧"
keywords: ["关键词1","关键词2"]
categories : ["进程管理"]
banner : "img/blogimg/default.png"
summary : "调度器是OS的核心部分，说白了就是CPU时间的管理员。调度器主要是负责某些就绪的进程来执行，不同的调度器根据不同的方法挑选出最适合运行的进程。通过查阅资料，目前Linux支持的调度器有RT scheduler、Deadline scheduler、CFS scheduler及Idle scheduler等。"
---

#### CFS调度器

调度器是OS的核心部分，说白了就是CPU时间的管理员。调度器主要是负责某些就绪的进程来执行，不同的调度器根据不同的方法挑选出最适合运行的进程。通过查阅资料，目前Linux支持的调度器有RT scheduler、Deadline scheduler、CFS scheduler及Idle scheduler等。

##### 什么是调度类

scheduling class目的是将调度器模块化，模块化提高了扩展性，从而方便添加新的调度器，从而一个系统中可以共存多个调度器。struct sched_class这个结构体将调度器公共的部分抽象，描述一个具体的调度类，系统核心调度代码通过struct sched_class结构体的成员调用具体调度类的核心算法。

![1588489542250](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588489542250.png)

1、next：next成员指向下一个调度类，也就是比自己低一个优先级的调度类。Linux里面，每一个调度类都是有明确的优先级关系，高优先级调度类所管理的进程会优先获得CPU的使用权。

2、enqueue_task：向该调度器管理的runqueue中添加一个进程，这个操作称为入队。

3、dequeue_task：向该调度器管理的runqueue中删除一个进程，这个操作称为出队。

4、check_preempt_curr：当一个进程被唤醒或者创建的时候，需要检查当前进程是否可以抢占当前CPU上正在运行的进程，如果可以抢占需要标记TIF_NEED_RESCHED flag

5、pick_next_task：从runqueue中选择一个最适合运行的task，这个操作是调度器相对核心的操作，因为什么时候调度最适合运行的进程是每个调度器需要关注的问题。

#### Linux中的调度类

Linux中主要包含dl_sched_class、rt_sched_class、fair_sched_class及idle_sched_class等调度类。每一个进程都对应一种调度策略，每一种调度策略对应一种调度类，比如说实时调度器以优先级为导向选择优先级高的进程运行。每一个进程创建之后，总是要选择一种调度策略，针对不同的调度策略，选择的调度器也是不一样的。

| 调度类           | 描述                                 | 调度策略                  |
| ---------------- | ------------------------------------ | ------------------------- |
| stop_sched_class | 优先级最高的线程，会中断所有其他线程 | 无，不需要调度普通进程    |
| dl_sched_class   | deadline调度器                       | SCHED_DEADLINE            |
| rt_sched_class   | 实时调度器                           | SCHED_FIFO、SCHED_RR      |
| fair_sched_class | 完全公平调度器                       | SCHED_NORMAL、SCHED_BATCH |
| idle_sched_class | idle task                            | SCHED_IDLE                |

 针对上面的调度类，系统中有明确的优先级概念，每一个调度类利用next成员构建单链表，优先级从高到低的顺序就是表格里的顺序。

Linux调度核心在选择下一个合适的进程运行的时候，会按照优先级的顺序遍历调度类的pick_next_task函数，所以说SCHED_FIFO调度策略的实时进程永远比SCHED_NORMAL调度策略的普通进程优先运行，下图是pick_next_task函数的部分代码，也可以体现出来，这个函数就是负责选择一个即将运行的进程。

![1588489373820](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588489373820.png)

![1588489420648](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588489420648.png)

for_each_class函数就是按照优先级顺序遍历所有的调度类，通过next指针遍历单链表。

#### 普通进程的优先级

CFS的英文是completely fair scheduler的简称，也就是完全公平调度器。CFS调度器跟别的调度器的不同之处在于没有时间片的概念，而是分配CPU使用时间的比例，比如说有两个相同优先级的进程在一个CPU上运行，那么每个进程都将会分配50%的CPU运行时间。那么现实不可能一直是所有进程同等优先级，有些进程的优先级比较高，CFS调度器的优先级是这样实现的，引入权重的概念，权重就是进程的优先级。各个进程按照权重的比例分配CPU时间。比如两个进程A和B，A的权重是1024，B的权重是2048，那么A获得的CPU时间比例是1024/（1024+2048）=33.3%，B获得的时间比例2048/（1024+2048）=66.7%，所以说，权重越大分配的时间比例越大，相当于优先级越高，引入权重的分配给进程的时间计算公式如下：

**分配给进程的时间=总的CPU时间*进程的权重/就绪队列所有进程权重之和**

CFS调度器针对优先级又提出了nice值的概念，其实nice值与权重是一一对应的关系。nice值是一个具体的数字，取值范围是[-20,19]。数值越小代表优先级越大，同时也代表权重越大，nice值和权重之间可以互相转换，内核提供了一个表格转换nice值和权重。

![1588493488061](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588493488061.png) 

数组的值可以看作是公式：weight=1024/1.25^nice计算得到，公式里面1.25取值依据是：进程每降低一个nice值，将多获得10%CPU的时间，公式中以1024权重为基准值计算得来，1024权重对应nice值为0，其权重称为NICE_0_LOAD。默认的情况下，大部分进程的权重基本都是NICE_0_LOAD。

#### 调度延迟

调度延迟就是保证每一个可运行进程都至少运行一次的时间间隔。例如，每个进程都运行10ms，系统中总共有2个进程，那么调度延迟就是20ms，如果有5个进程，那么调度延迟就是50ms。如果现在保证调度延迟不变，固定是6ms，那么如果系统中有2个进程，那么每个进程运行3ms，如果有6个进程，那么每个进程运行1ms，随着进程的增加，每个进程分配的时间在减少，进程调度过于频繁，上下文切换时间开销就会变大。因此，CFS调度器的调度延迟时间的设定并不是固定的，当系统处于就绪态的进程少于定值（默认为8）的时候，调度延迟也是固定一个值不变（默认值6ms）。当系统就绪态的进程个数超过这个值时，我们要保证每个进程至少运行一定的时间才让出CPU，并且这个"这个至少一定的时间"被称为最小粒度时间，在CFS默认设置中，最小粒度时间是0.75ms，用变量sysctl_sched_min_granularity记录，因此，调度周期是一个动态变化的值，调度周期计算函数是__sched_period()。

![1588498299706](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588498299706.png)

其中nr_running是系统中就绪进程数量，当超过sched_nr_latency时，就无法保证调度延迟，因此转为保证调度最小粒度。如果nr_running并没有超过sched_nr_latency，那么调度周期就等于调度延迟也就是sysctl_sched_latency(6ms)。

#### 虚拟时间

 CFS调度器的目标是保证每一个进程的完全公平调度。可以把它比作一位母亲，有很多孩子（进程），但是，手里只有一个玩具（CPU），需要公平的分配给孩子玩。假设有两个孩子，那么一个玩具公平给两个孩子玩，就是第一个孩子玩5分钟后第二个孩子玩5分钟，以此循环。CFS调度器就是这样记录每一个进程的执行时间，保证每个进程获取CPU执行时间的公平。因此，哪个进程运行的时间最少，应该让哪个进程运行。

比如，调度周期是6ms，系统一共2个相同优先级的进程A和B，那么每个进程都将在6ms周期内各运行3ms。如果进程A和B，它们的权重分别是1024和820（nice值分别为0和1）。进程A获得的运行时间是6*1024/（1024+820）=3.3ms，进程B获得的运行时间是6 *1024/（1024+820）=2.7ms。进程A的CPU使用比例是3.3/6=55%，进程B的CPU使用比例是2.7/6=45%，计算的结果也符合"进程每降低一个nice值，将多获得10%的CPU时间"。很明显，2个进程的实际执行时间是不相等的，但是CFS想保证每个进程运行时间相等。因此，CFS引入了虚拟时间的概念，也就是说2.7ms和3.3ms经过一个公式的转换可以得到一样的值，这个转换后的值称为虚拟时间，这样，CFS只需要保证每个进程运行的虚拟时间是相等的即可，虚拟时间vriture_runtime和实际时间（wall time）转换公式如下：

**vriture_runtime=wall time*（NICE_0_LOAD/weight）**

进程A的虚拟时间3.3*1024/1024=3.3ms，可以明确看到nice值为0的进程的虚拟时间和实际时间是相等的，进程B的虚拟时间2.7 *1024/820=3.3ms，可以看见尽管A和B进程的权重值不一样，但是计算得到的虚拟时间是一样的，所以说CFS保证每一个进程获得执行虚拟时间一致即可。在选择下一个即将运行的进程时，只需要找到虚拟时间最小的进程即可。为了避免浮点数运算，因此采用先放大再缩小的方法以保证计算精度，内核又对公式做了如下转换。

```c
                                     NICE_0_LOAD
    vriture_runtime = wall_time * ----------------
                                        weight
      
                                       NICE_0_LOAD * 2^32
                    = (wall_time * -------------------------) >> 32
                                            weight
                                                                                            2^32
                    = (wall_time * NICE_0_LOAD * inv_weight) >> 32        (inv_weight = ------------ )
                                                                                            weight 
```

权重的值已经计算保存在sched_prio_to_weight数组中，根据这个数组可以很容易计算inv_weight的值。内核中使用sched_prio_to_wmult数组保存inv_weight的值。计算公式是：sched_prio_to_wmult[i] = 2^32/sched_prio_to_weight[i]。

![1588502847113](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588502847113.png)

系统中使用struct_load_weight结构体描述进程的权重信息。weight代表进程的权重，inv_weight等于2^32/weight。

![1588502995080](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588502995080.png)

将实际时间转换成虚拟时间的实现函数是calc_delta_fair()。calc_delta_fair()调用__calc_delta()函数，calc_delta()主要功能是实现如下公式的计算。

![1589031968295](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1589031968295.png)

![1588503074695](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588503074695.png)

按照上面说的理论，calc_delta_fair()函数调用__calc_delta()的时候传递的weight参数是NICE_0_LOAD，lw参数是进程对应的struct load_weight结构体。

![1588503190044](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588503190044.png)

> 按照之前的理论，nice值为0（权重是NICE_0_LOAD）的进程的虚拟时间和实际时间是相等的。因此如果进程的权重是NICE_0_LOAD，进程对应的虚拟时间就不用计算。调用__calc_delta()函数。 	
>
> Linux通过struct task_struct结构体描述每一个进程。但是调度类管理和调度的单位是调度实体，并不是task_struct。在支持组调度的时候，一个组也会抽象成一个调度实体，它并不是一个task。所以，我们在struct task_struct结构体中可以找到以下不同调度类的调度实体。	
>
> ![1588646235793](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588646235793.png)

![1588646215121](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588646215121.png)

se、rt、dl分别对应CFS调度器、RT调度器、Deadline调度器的调度实体。

struct sched_entity结构体描述调度实体，包括struct load_weight用来记录权重信息，除此之外我们一直关心的时间信息，也要一起记录，struct sched_entity结构体简化后如下：

![1588648551711](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588648551711.png)

load：权重信息，在计算虚拟时间的时候会用到inv_weight成员

run_node：CFS调度器的每个就绪队列维护了一颗红黑树，上面挂满了就绪等待执行的task，run_node就是挂载点

on_rq：调度实体se加入就绪队列后，on_rq置1，从就绪队列删除后，on_rq置0

sum_exec_runtime：调度实体已经运行实际时间总和

vruntime：调度实体已经运行的虚拟时间总和

####  就绪队列

系统中每个CPU都会有一个全局的就绪队列（cpu runqueue），使用struct rq结构体描述，它是per-cpu类型，就是每个CPU上都会有一个struct rq结构体。每一个调度类也有属于自己管理的就绪队列。比如struct cfs_rq就是CFS调度类的就绪队列，管理就绪态的struct sched_entity调度实体，后续通过pick_next_task接口从就绪队列中选择最适合运行的调度实体，也就是虚拟时间最小的调度实体。struct rt_rq是实时调度器就绪队列，struct dl_rq是Deadline调度器就绪队列。

![1588650556813](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588650556813.png)

![1588650567127](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588650567127.png)

![1588650611413](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588650611413.png)

![1588650678559](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588650678559.png)

load：就绪队列权重，就绪队列管理的所有调度实体权重之和

nr_running：就绪队列上调度实体的个数

min_vruntime：跟踪就绪队列上所有调度实体的最小虚拟时间

tasks_timeline：用于跟踪调度实体按虚拟时间大小排序的红黑树的信息（包含红黑树的根以及红黑树中最左边结点）

CFS维护了一个按照虚拟时间排序的红黑树，所有可运行的调度实体按照p->se.vruntime排序插入红黑树。

![1588661897832](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588661897832.png)

CFS选择红黑树最左边的进程运行，随着系统时间的推移，原来左边运行过的进程慢慢的会移到红黑树的右边，原来右边的进程也会最终跑到最左边，因此红黑树中的每个进程都有机会运行。

总结一下，Linux中所有的进程使用task_struct描述，task_struct包含很多进程相关的信息，比如优先级、进程状态以及调度实体等，但是，每一个调度类并不是直接管理task_struct，而是引入调度实体的概念。CFS调度器使用sched_entity跟踪调度信息，CFS调度器使用cfs_rq跟踪就绪队列信息以及管理就绪态调度实体，并维护一颗按照虚拟时间排序的红黑树。task_timeline->rb_root是红黑树的根，tasks_timeline->rb_leftmost指向红黑树中最左边的调度实体，即虚拟时间最小的调度实体（为了更快的选择最合适运行的调度实体，因此rb_leftmost相当于一个缓存）。每个就绪态的调度实体sched_entity包含插入红黑树中使用的结点rb_node，同时vruntime成员记录已经运行的虚拟时间，将这几个数据结构简单梳理。

![1588665582962](C:\Users\mmh\AppData\Roaming\Typora\typora-user-images\1588665582962.png)