---
title: "CFS调度器之时间记账"
date: 2020-09-01T19:48:41+08:00
author: "zxz"
keywords: ["CFS","时间记账"]
categories : ["新手上路"]
banner : "img/blogimg/default.png"
summary : "本文主要带领大家在2.6内核中了解CFS调度器的时间记账功能"
---
# 1.调度器实体结构
&emsp;&emsp;Linux中所有的进程使用task_struct描述。task_struct包含很多进程相关的信息（例如，优先级、进程状态以及调度实体等）。但是，每一个调度类并不是直接管理task_struct，而是引入调度实体的概念。CFS调度器使用sched_entity跟踪调度信息。CFS调度器使用cfs_rq跟踪就绪队列信息以及管理就绪态调度实体，并维护一棵按照虚拟时间排序的红黑树。
```
struct sched_entity {
	struct load_weight	load;		//权重信息
	struct rb_node		run_node;	//CFS调度器的每个就绪队列维护了一颗红黑树，上面挂满了就绪等待执行的task，run_node就是挂载点
	struct list_head	group_node;	
	unsigned int		on_rq;		//调度实体se加入就绪队列后，on_rq置1。从就绪队列删除后，on_rq置0

	u64			exec_start;
	u64			sum_exec_runtime;	//调度实体已经运行实际时间总合
	u64			vruntime;			//调度实体已经运行的虚拟时间总合
	u64			prev_sum_exec_runtime;

	u64			last_wakeup;
	u64			avg_overlap;

	u64			nr_migrations;

	u64			start_runtime;
	u64			avg_wakeup;

    ..........
}
```
# 2.虚拟时间
## 2.1 vruntime
CFS通过每个进程的虚拟运行时间（vruntime）来衡量哪个进程最值得被调度。

虚拟运行时间是通过进程的实际运行时间和进程的权重（weight）计算出来的。

就绪队列上虚拟时间的信息：
```
struct cfs_rq {
	struct load_weight load;	//所有进程累计负荷值
	unsigned long nr_running;	//当前就绪队列的进程数

	u64 exec_clock;
	u64 min_vruntime;			//就绪队列上所有调度实体的最小虚拟时间

	.................
```
## 2.2 update_curr计算进程虚拟时间
先给出update_curr函数源码：
```
static void update_curr(struct cfs_rq *cfs_rq)
{
	struct sched_entity *curr = cfs_rq->curr;//确定就绪队列当前执行的进程curr
	u64 now = rq_of(cfs_rq)->clock;//rq_of返回了cfs_rq所在的全局就绪队列并获取clock
	unsigned long delta_exec;
	//如果就绪队列没有进程执行，无事可做
	if (unlikely(!curr))
		return;

	//内核计算当前和上一次更新负荷权重时两次的时间的差值
	delta_exec = (unsigned long)(now - curr->exec_start);
	if (!delta_exec)
		return;
	//update_curr计算出当前进程的执行时间放在delta_exec中送给__update_curr,后者根据当前可运行进程总数对运行时间进行加权计算。
	__update_curr(cfs_rq, curr, delta_exec);
	//重新更新更新启动时间exec_start为now, 以备下次计算时使用
	curr->exec_start = now;

	if (entity_is_task(curr)) {
		struct task_struct *curtask = task_of(curr);

		trace_sched_stat_runtime(curtask, delta_exec, curr->vruntime);
		cpuacct_charge(curtask, delta_exec);
		account_group_exec_runtime(curtask, delta_exec);
	}
}
```

所有与虚拟时钟有关的计算都在update_curr中执行,update_curr的流程如下
* 首先计算进程当前时间与上次启动时间的差值
* 通过负荷权重和当前时间模拟出进程的虚拟运行时钟
* 重新设置cfs的min_vruntime保持其单调性

### 2.2.1 计算时间差
在update_curr函数中：

首先确定就绪队列当前执行的进程curr:
```
struct sched_entity *curr = cfs_rq->curr;
```
获取当前的时间信息：
```
u64 now = rq_of(cfs_rq)->clock;
unsigned long delta_exec;
```
判断就绪队列没有进程执行：
```
if (unlikely(!curr))
	return;
```

内核计算当前和上一次更新负荷权重时两次的时间的差值:
```
delta_exec = (unsigned long)(now - curr->exec_start);
if (!delta_exec)
	return;
```
重新更新更新启动时间exec_start为now, 以备下次计算时使用:
```
curr->exec_start = now;
```
### 2.2.2 计算出虚拟运行时间
首先update_curr计算出当前进程的执行时间放在delta_exec中送给__update_curr,后者根据当前可运行进程总数对运行时间进行加权计算。

下列为__update_curr函数：
```
__update_curr(struct cfs_rq *cfs_rq, struct sched_entity *curr,
	      unsigned long delta_exec)
{
	unsigned long delta_exec_weighted;

	schedstat_set(curr->exec_max, max((u64)delta_exec, curr->exec_max));
	
	curr->sum_exec_runtime += delta_exec;
	schedstat_add(cfs_rq, exec_clock, delta_exec);
	delta_exec_weighted = calc_delta_fair(delta_exec, curr);//计算虚拟时间vruntime

	curr->vruntime += delta_exec_weighted;
	update_min_vruntime(cfs_rq);
}
```
__update_curr函数：

* 首先更新了调度实体已经运行时间总和
* 调用calc_delta_fair(delta_exec, curr)函数计算虚拟时间总和
* 重新设置cfs_rq->min_vruntime
#### 2.2.2.1 更新调度实体已经运行时间总和
```
curr->sum_exec_runtime += delta_exec;
```
#### 2.2.2.1 calc_delta_fair函数

将实际时间转换成虚拟时间的实现函数是calc_delta_fair()。
```
alc_delta_fair(unsigned long delta, struct sched_entity *se)
{
	if (unlikely(se->load.weight != NICE_0_LOAD))
		delta = calc_delta_mine(delta, NICE_0_LOAD, &se->load);

	return delta;
}
```
calc_delta_fair()调用calc_delta_mine()函数

calc_delta_mine()主要功能是实现如下公式
```
calc_delta() = (delta_exec * weight * lw->inv_weight) >> 32 
```
下面为calc_delta_mine函数的源码：
```
static unsigned long
calc_delta_mine(unsigned long delta_exec, unsigned long weight,
		struct load_weight *lw)
{
	u64 tmp;

	if (!lw->inv_weight) {
		if (BITS_PER_LONG > 32 && unlikely(lw->weight >= WMULT_CONST))
			lw->inv_weight = 1;
		else
			lw->inv_weight = 1 + (WMULT_CONST-lw->weight/2)
				/ (lw->weight+1);
	}
	tmp = (u64)delta_exec * weight;
	/*
	 * Check whether we'd overflow the 64-bit multiplication:
	 */
	if (unlikely(tmp > WMULT_CONST))
		tmp = SRR(SRR(tmp, WMULT_SHIFT/2) * lw->inv_weight,
			WMULT_SHIFT/2);
	else
		tmp = SRR(tmp * lw->inv_weight, WMULT_SHIFT);

	return (unsigned long)min(tmp, (u64)(unsigned long)LONG_MAX);
}
```
#### 2.2.2.1 重新设置cfs_rq->min_vruntime
通过update_min_vruntime函数进行更新：


通过分析update_min_vruntime函数设置cfs_rq->min_vruntime的流程如下:
* 首先检测cfs就绪队列上是否有活动进程curr, 以此设置vruntime的值,如果cfs就绪队列上没有活动进程curr,就设置vruntime为curr->vruntime;否则又活动进程就设置为vruntime为cfs_rq的原min_vruntime;
* 接着检测cfs的红黑树上是否有最左节点,即等待被调度的节点,重新设置vruntime的值为curr进程和最左进程rb_leftmost的vruntime较小者的值
* 为了保证min_vruntime单调不减,只有在vruntime超出的cfs_rq->min_vruntime的时候才更新update_min_vruntime依据当前进程和待调度的进程的vruntime值, 设置出一个可能的vruntime值,但是只有在这个可能的vruntime值大于就绪队列原来的min_vruntime的时候, 才更新就绪队列的min_vruntime, 利用该策略, 内核确保min_vruntime只能增加, 不能减少。
```
static void update_min_vruntime(struct cfs_rq *cfs_rq)
{
	u64 vruntime = cfs_rq->min_vruntime;//队列的虚拟时间

	if (cfs_rq->curr)
		vruntime = cfs_rq->curr->vruntime;//队列虚拟时间==进程的虚拟时间
	//检查红黑树是否有最左的节点，即是否有进程在树上等待
	if (cfs_rq->rb_leftmost) {
		//获取最左节点调度实体信息se，se中存储了vruntime
		struct sched_entity *se = rb_entry(cfs_rq->rb_leftmost,
						   struct sched_entity,
						   run_node);
		 /*  如果就绪队列上没有curr进程
         *  则vruntime设置为树种最左结点的vruntime
         *  否则设置vruntiem值为cfs_rq->curr->vruntime和se->vruntime的最小值
         */
		if (!cfs_rq->curr)
			vruntime = se->vruntime;/*  此时vruntime的原值为cfs_rq->min_vruntime*/
		else
			vruntime = min_vruntime(vruntime, se->vruntime); /* 此时vruntime的原值为cfs_rq->curr->vruntime*/
	}
 	/* 
     * 为了保证min_vruntime单调不减
     * 只有在vruntime超出的cfs_rq->min_vruntime的时候才更新
     */
	cfs_rq->min_vruntime = max_vruntime(cfs_rq->min_vruntime, vruntime);
}
```
