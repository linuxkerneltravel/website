---
title: "CFS调度算法(二)--进程选择"
date: 2020-09-08T22:04:51+08:00
author: "zxz"
keywords: ["CFS","进程选择"]
categories : ["新手上路"]
banner : "img/blogimg/default.png"
summary : "CFS如何进行进程选择"
---
# CFS调度算法(二)--进程选择
CFS利用rbtree来组织进程可运行队列，并迅速找到其最小vruntime。红黑树的排序过程是进程的vruntime来进行计算的。

* 在程序运行时, 其vruntime稳定地增加,他在红黑树中总是向右移动的.因为越重要的进程vruntime增加的越慢,因此他们向右移动的速度也越慢,这样其被调度的机会要大于次要进程。
* 如果进程进入睡眠, 则其vruntime保持不变.因为每个队列min_vruntime同时会单调增加, 那么当进程从睡眠中苏醒,在红黑树中的位置会更靠左,因为其键值相对来说变得更小了。
# 1.入队操作
向就绪队列中放置新进程

实现函数：enqueue_task_fair()

kernel/sched/fair.c

enqueue_task_fair的执行流程如下:

* 如果通过struct sched_entity的on_rq成员判断进程已经在就绪队列上, 则无事可做.
* 否则, 具体的工作委托给enqueue_entity完成,其中内核会借机用update_curr更新统计量

在enqueue_entity内部如果需要会调用__enqueue_entity将进程插入到CFS红黑树中合适的结点
```
static void
enqueue_task_fair(struct rq *rq, struct task_struct *p, int wakeup, bool head)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se = &p->se;
	int flags = 0;

	if (wakeup)
		flags |= ENQUEUE_WAKEUP;
	if (p->state == TASK_WAKING)
		flags |= ENQUEUE_MIGRATE;

	for_each_sched_entity(se) {
		if (se->on_rq)/*判断进程已经在就绪队列上就什么都不做*/
			break;
     /* 获取到当前进程所在的cfs_rq就绪队列 */
		cfs_rq = cfs_rq_of(se);
     /* 内核委托enqueue_entity完成真正的插入工作 */
		enqueue_entity(cfs_rq, se, flags);
		flags = ENQUEUE_WAKEUP;
	}

	hrtick_update(rq);
}
```
## 1.1循环所有调度实体
* 如果通过struct sched_entity的on_rq成员判断进程已经在就绪队列上,则无事可做.
* 否则, 具体的工作委托给enqueue_entity完成,其中内核会借机用update_curr更新统计量。
```
#define for_each_sched_entity(se) \
		for (; se; se = se->parent)
```
## 1.2向树中加入进程
enqueue_entity完成了进程真正的入队操作, 其具体流程如下所示
* 更新一些统计统计量, update_curr, update_cfs_shares等
* 如果进程此前是在睡眠状态, 则调用place_entity中首先会调整进程的虚拟运行时间
* 最后如果进程最近在运行,其虚拟运行时间仍然有效,那么则直接用__enqueue_entity加入到红黑树

实现函数：enqueue_entity()

kernel/sched_fair.c
```
#define ENQUEUE_WAKEUP	1
#define ENQUEUE_MIGRATE 2

static void
enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se, int flags)
{
	/*
	 * Update the normalized vruntime before updating min_vruntime
	 * through callig update_curr().
	 */
	
	//如果当前进程之前已经是可运行状态不是被唤醒的那么其虚拟运行时间要增加
	if (!(flags & ENQUEUE_WAKEUP) || (flags & ENQUEUE_MIGRATE))
		se->vruntime += cfs_rq->min_vruntime;

	/*
	 * Update run-time statistics of the 'current'.
	 */
	//更新进程的统计信息
	update_curr(cfs_rq);
	account_entity_enqueue(cfs_rq, se);
	//当前进程之前在睡眠刚被唤醒
	if (flags & ENQUEUE_WAKEUP) {
		place_entity(cfs_rq, se, 0);//调整进程的虚拟运行时间
		enqueue_sleeper(cfs_rq, se);
	}

	update_stats_enqueue(cfs_rq, se);
	check_spread(cfs_rq, se);
	//将进程插入到rbtree中
	if (se != cfs_rq->curr)
		__enqueue_entity(cfs_rq, se);
}
```
### 1.2.1 place_entity()
* 如果进程此前在睡眠, 那么则调用place_entity处理其虚拟运行时间

在休眠进程被唤醒时重新设置vruntime值，以min_vruntime值为基础，给予一定的补偿，但不能补偿太多

* 新进程创建完成后, 也是通过place_entity完成其虚拟运行时间vruntime的设置的

kernel/sched_fair.c
```
static void
place_entity(struct cfs_rq *cfs_rq, struct sched_entity *se, int initial)//initial标记是睡醒的进程还是新进程
{
	u64 vruntime = cfs_rq->min_vruntime;

	/*
	 * The 'current' period is already promised to the current tasks,
	 * however the extra weight of the new task will slow them down a
	 * little, place the new task so that it fits in the slot that
	 * stays open at the end.
	 * 如果是新进程第一次要入队, 那么就要初始化它的vruntime
     * 一般就把cfsq的vruntime给它就可以
     * 但是如果当前运行的所有进程被承诺了一个运行周期
     * 那么则将新进程的vruntime后推一个他自己的slice
     * 实际上新进程入队时要重新计算运行队列的总权值
     * 总权值显然是增加了，但是所有进程总的运行时期并不一定随之增加
     * 则每个进程的承诺时间相当于减小了，就是减慢了进程们的虚拟时钟步伐。
	 */

	//新进程
	if (initial && sched_feat(START_DEBIT))
		vruntime += sched_vslice(cfs_rq, se);

	/* sleeps up to a single latency don't count. */
	//休眠进程
	if (!initial && sched_feat(FAIR_SLEEPERS)) {
		unsigned long thresh = sysctl_sched_latency;//一个调度周期

		/*
		 * Convert the sleeper threshold into virtual time.
		 * SCHED_IDLE is a special sub-class.  We care about
		 * fairness only relative to other SCHED_IDLE tasks,
		 * all of which have the same weight.
		 */
		if (sched_feat(NORMALIZED_SLEEPER) && (!entity_is_task(se) ||
				 task_of(se)->policy != SCHED_IDLE))
			thresh = calc_delta_fair(thresh, se);

		/*
		 * Halve their sleep time's effect, to allow
		 * for a gentler effect of sleepers:
		 */
		//如果设置了GENTLE_FAIR_SLEEPERS
		if (sched_feat(GENTLE_FAIR_SLEEPERS))
			thresh >>= 1;	//补偿为调度周期一半

		vruntime -= thresh;
	}

	/* ensure we never gain time by being placed backwards. */
	//唤醒了已经存在的进程，则单调肤赋值
	vruntime = max_vruntime(se->vruntime, vruntime);

	se->vruntime = vruntime;
}
```
==initial参数为0==

因为进程睡眠后，vruntime就不会增加了，当它醒来后不知道过了多长时间，可能vruntime已经比 min_vruntime小了很多，如果只是简单的将其插入到就绪队列中，它将拼命追赶min_vruntime，因为它总是在红黑树的最左面。如果这 样，它将会占用大量的CPU时间，导致红黑树右边的进程被饿死。但是我们又必须及时响应醒来的进程，因为它们可能有一些工作需要立刻处理，所以系统采取了 一种折衷的办法，将当前cfs_rq->min_vruntime时间减去sysctl_sched_latency赋给vruntime，这时它 会被插入到就绪队列的最左边。

==initial参数为1==

Linux内核需要根据新加入的进程的权重决策一下应该何时调度该进程，而不能任意进程都来抢占当前队列中靠左的进程，因为必须保证就绪队列中的所有进程尽量得到他们应得的时间响应， sched_vslice函数就将其负荷权重转换为等价的虚拟时间.
```
static u64 sched_vslice(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	return calc_delta_fair(sched_slice(cfs_rq, se), se);
}
```
### 1.2.2 真正的插入操作
进行繁重的插入操作，把数据项真正插入到rbtree中。

实现函数：__enqueue_entity()

kernel/sched_fair.c
```
static void __enqueue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	struct rb_node **link = &cfs_rq->tasks_timeline.rb_node;
	struct rb_node *parent = NULL;
	struct sched_entity *entry;
	s64 key = entity_key(cfs_rq, se);
	int leftmost = 1;

	/*
	 * Find the right place in the rbtree:
	 * 从红黑树中找到se所应该在的位置
     * 同时leftmost标识其位置是不是最左结点
     * 如果在查找结点的过程中向右走了, 则置leftmost为0
     * 否则说明一直再相左走, 最终将走到最左节点, 此时leftmost恒为1
	 */
	while (*link) {
		parent = *link;
		entry = rb_entry(parent, struct sched_entity, run_node);
		/*
		 * We dont care about collisions. Nodes with
		 * the same key stay together.
		 * 以se->vruntime值为键值进行红黑树结点的比较
		 */
		if (key < entity_key(cfs_rq, entry)) {
			link = &parent->rb_left;
		} else {
			link = &parent->rb_right;
			leftmost = 0;
		}
	}
	/*
	 * Maintain a cache of leftmost tree entries (it is frequently
	 * used):
	 *  如果leftmost为1, 说明se是红黑树当前的最左结点, 即vruntime最小
     * 那么把这个节点保存在cfs就绪队列的rb_leftmost域中
	 */
	if (leftmost)
		cfs_rq->rb_leftmost = &se->run_node;
/*  将新进程的节点加入到红黑树中  */
	rb_link_node(&se->run_node, parent, link);
 /*  为新插入的结点进行着色  */
	rb_insert_color(&se->run_node, &cfs_rq->tasks_timeline);
}
```
# 2.出队操作
将任务从就绪队列中移除其执行的过程正好跟enqueue_task_fair的思路相同, 只是操作刚好相反。

dequeue_task_fair的执行流程如下：

* 如果通过struct sched_entity的on_rq成员判断进程已经在就绪队列上, 则无事可做。
* 否则, 具体的工作委托给dequeue_entity完成,在enqueue_entity内部如果需要会调用__dequeue_entity将进程插入到CFS红黑树中合适的结点

```
static void dequeue_task_fair(struct rq *rq, struct task_struct *p, int sleep)
{
	struct cfs_rq *cfs_rq;
	struct sched_entity *se = &p->se;

	for_each_sched_entity(se) {
		cfs_rq = cfs_rq_of(se);
		dequeue_entity(cfs_rq, se, sleep);
		/* Don't dequeue parent if it has other entities besides us */
		if (cfs_rq->load.weight)
			break;
		sleep = 1;
	}

	hrtick_update(rq);
}
```
## 2.1 dequeue_entity()
kernel/sched_fair.c

```
static void
dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se, int sleep)
{
	/*
	 * Update run-time statistics of the 'current'.
	 */
	update_curr(cfs_rq);

	update_stats_dequeue(cfs_rq, se);
	if (sleep) {
#ifdef CONFIG_SCHEDSTATS
		if (entity_is_task(se)) {
			struct task_struct *tsk = task_of(se);

			if (tsk->state & TASK_INTERRUPTIBLE)
				se->sleep_start = rq_of(cfs_rq)->clock;
			if (tsk->state & TASK_UNINTERRUPTIBLE)
				se->block_start = rq_of(cfs_rq)->clock;
		}
#endif
	}

	clear_buddies(cfs_rq, se);

	if (se != cfs_rq->curr)
		__dequeue_entity(cfs_rq, se);
	account_entity_dequeue(cfs_rq, se);
	update_min_vruntime(cfs_rq);

	/*
	 * Normalize the entity after updating the min_vruntime because the
	 * update can refer to the ->curr item and we need to reflect this
	 * movement in our normalized position.
	 */
	if (!sleep)
		se->vruntime -= cfs_rq->min_vruntime;
}
```
真正的出队操作
```
static void __dequeue_entity(struct cfs_rq *cfs_rq, struct sched_entity *se)
{
	if (cfs_rq->rb_leftmost == &se->run_node) {
		struct rb_node *next_node;

		next_node = rb_next(&se->run_node);
		cfs_rq->rb_leftmost = next_node;
	}

	rb_erase(&se->run_node, &cfs_rq->tasks_timeline);
}
```
