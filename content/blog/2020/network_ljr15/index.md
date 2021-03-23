---
title: "Linux内核网络数据发送（五）——排队规则"
date: 2020-11-09T10:07:28+08:00
author: "梁金荣"
keywords: ["内核网络"]
categories : ["内核网络"]
banner : "img/blogimg/ljrimg16.jpg"
summary : "本文将分析**通用的数据包调度程序**（generic packet scheduler）的工作过程，通过分析 `qdisc_run_begin()`、`qdisc_run_end()`、`__ qdisc_run()` 和 `sch_direct_xmit()` 函数，了解内核如何一层层将数据传递给驱动程序，最后进行了监控和调优。"
---

# 1. 前言

本文将分析**通用的数据包调度程序**（generic packet scheduler）的工作过程，通过分析 `qdisc_run_begin()`、`qdisc_run_end()`、`__ qdisc_run()` 和 `sch_direct_xmit()` 函数，了解内核如何一层层将数据传递给驱动程序，最后进行了监控和调优。

# 2. `qdisc_run_begin()` and `qdisc_run_end()`：仅设置 qdisc 状态位

定义在include/net/sch_generic.h:

```c
static inline bool qdisc_run_begin(struct Qdisc *qdisc)
{
        if (qdisc_is_running(qdisc))
                return false;
        qdisc->__state |= __QDISC___STATE_RUNNING;
        return true;
}

static inline void qdisc_run_end(struct Qdisc *qdisc)
{
        qdisc->__state &= ~__QDISC___STATE_RUNNING;
}
```

- `qdisc_run_begin()` 检查 qdisc 是否设置了`__QDISC___STATE_RUNNING` 状态位。如果设置了，直接返回 `false`；否则，设置此状态位，然后返回 `true`。
- `qdisc_run_end()` 执行相反的操作，清除此状态位。

这两个函数都**只是设置状态位，并没有真正干活**。真正的处理过程是从 `__qdisc_run()` 开始的。



# 3. `__qdisc_run()`：真正的 qdisc 执行入口

先看`__qdisc_run()`函数：

```c
void __qdisc_run(struct Qdisc *q)
{
        int quota = weight_p;

        while (qdisc_restart(q)) { // 从队列取出一个 skb 并发送，剩余队列不为空时返回非零

                // 如果发生下面情况之一，则延后处理：
                // 1. quota 用尽
                // 2. 其他进程需要 CPU
                if (--quota <= 0 || need_resched()) {
                        __netif_schedule(q);
                        break;
                }
        }

        qdisc_run_end(q);          // 清除 RUNNING 状态位
}
```

函数首先获取 `weight_p`，这个变量通常是通过 sysctl 设置的，收包路径也会用到。这个循环做两件事：

1. 在 `while` 循环中调用 `qdisc_restart()`，直到它返回 `false`（或触发下面的中断）。
2. 判断是否还有 quota，或 `need_resched()` 是否返回 `true`。其中任何一个为真， 将调用 `__netif_schedule()` 然后跳出循环。

> 用户程序调用 `sendmsg` **系统调用之后，内核便接管了执行过程，一路执行到这里，用户程序一直在累积系统时间（system time）**。

- 如果用户程序在内核中用完其 time quota，`need_resched()` 将返回 `true`。
- 如果仍有 quota，且用户程序的时间片尚未使用，则将再次调用 `qdisc_restart()`。

先来看看 `qdisc_restart(q)`是如何工作的，然后将深入研究`__netif_schedule(q)`。



# 4. `qdisc_restart`：从 qdisc 队列中取包，发送给网络驱动

看`qdisc_restart()`函数:

```c
/*
 * NOTE: Called under qdisc_lock(q) with locally disabled BH.
 *
 * __QDISC_STATE_RUNNING guarantees only one CPU can process
 * this qdisc at a time. qdisc_lock(q) serializes queue accesses for this queue.
 *
 *  netif_tx_lock serializes accesses to device driver.
 *
 *  qdisc_lock(q) and netif_tx_lock are mutually exclusive,
 *  if one is grabbed, another must be free.
 *
 * Returns to the caller:
 *                                0  - queue is empty or throttled.
 *                                >0 - queue is not empty.
 */
static inline int qdisc_restart(struct Qdisc *q)
{
        struct sk_buff      *skb = dequeue_skb(q);
        if (!skb)
            return 0;

        spinlock_t          *root_lock = qdisc_lock(q);
        struct net_device   *dev = qdisc_dev(q);
        struct netdev_queue *txq = netdev_get_tx_queue(dev, skb_get_queue_mapping(skb));

        return sch_direct_xmit(skb, q, dev, txq, root_lock);
}
```

`qdisc_restart()` 函数开头的注释非常有用，描述了用到的三个锁：

1. `__QDISC_STATE_RUNNING` 保证了同一时间只有一个 CPU 可以处理这个 qdisc。
2. `qdisc_lock(q)` 将**访问此 qdisc** 的操作顺序化。
3. `netif_tx_lock` 将**访问设备驱动**的操作顺序化。

函数逻辑：

1. 首先调用 `dequeue_skb()` 从 qdisc 中取出要发送的 skb。如果队列为空，返回 0， 这将导致上层的 `qdisc_restart()` 返回 `false`，继而退出 `while` 循环。
2. 如果 skb 不为空，接下来获取 qdisc 队列锁，然后找到相关的发送设备 `dev` 和发送队列 `txq`，最后带着这些参数调用 `sch_direct_xmit()`。

先来看 `dequeue_skb()`，然后再回到 `sch_direct_xmit()`。

## 4.1 `dequeue_skb()`：从 qdisc 队列取待发送 skb

 `dequeue_skb()`定义在 net/sched/sch_generic.c:

```c
static inline struct sk_buff *dequeue_skb(struct Qdisc *q)
{
    struct sk_buff      *skb = q->gso_skb;   // 待发送包
    struct netdev_queue *txq = q->dev_queue; // 之前发送失败的包所在的队列

    if (unlikely(skb)) {
        /* check the reason of requeuing without tx lock first */
        txq = netdev_get_tx_queue(txq->dev, skb_get_queue_mapping(skb));

        if (!netif_xmit_frozen_or_stopped(txq)) {
            q->gso_skb = NULL;
            q->q.qlen--;
        } else
            skb = NULL;
    } else {
        if (!(q->flags & TCQ_F_ONETXQUEUE) || !netif_xmit_frozen_or_stopped(txq))
            skb = q->dequeue(q);
    }

    return skb;
```

函数首先声明一个 `struct sk_buff *skb` 变量，这是接下来要处理的数据。这个变量后面会依不同情况而被赋不同的值，最后作为返回值返回给调用方。

变量 `skb` 初始化为 qdisc 的 `gso_skb` 字段，这是**之前由于发送失败而重新入队的数据**。

接下来分为两种情况，根据 `skb = q->gso_skb` 是否为空：

1. 如果不为空，会将之前重新入队的 skb 出队，作为待处理数据返回。

   1. 检查发送队列是否已停止。
   2. 如果队列未停止，则 `gso_skb` 字段置空，队列长度减 1，返回 skb。
   3. 如果队列已停止，则 `gso_skb` 不动，返回空。

2. 如果为空（即之前没有数据重新入队），则从要处理的 qdisc 中取出一个新 skb，作为待处理数据返回。

   进入另一个 tricky 的 if 语句，如果：

   1. qdisc 不是单发送队列，或
   2. 发送队列未停止工作

   则调用 qdisc 的 `dequeue()` 方法获取新数据并返回。dequeue 的内部实现依 qdisc 的实现和功能而有所不同。

该函数最后返回变量 `skb`，这是接下来要处理的数据包。

## 4.2 `sch_direct_xmit()`：发送给网卡驱动

 `sch_direct_xmit()`定义在 net/sched/sch_generic.c，这是将数据向下发送到网络设备的重要一步。

```c
/*
 * Transmit one skb, and handle the return status as required. Holding the
 * __QDISC_STATE_RUNNING bit guarantees that only one CPU can execute this
 * function.
 *
 * Returns to the caller:
 *                                0  - queue is empty or throttled.
 *                                >0 - queue is not empty.
 */
int sch_direct_xmit(struct sk_buff *skb, struct Qdisc *q,
                    struct net_device *dev, struct netdev_queue *txq,
                    spinlock_t *root_lock)
{
        int ret = NETDEV_TX_BUSY;

        spin_unlock(root_lock);
        if (!netif_xmit_frozen_or_stopped(txq))
            ret = dev_hard_start_xmit(skb, dev, txq);
        spin_lock(root_lock);

        if (dev_xmit_complete(ret)) {                    // 1. 驱动发送成功
            ret = qdisc_qlen(q);                         //    将 qdisc 队列的剩余长度作为返回值
        } else if (ret == NETDEV_TX_LOCKED) {            // 2. 驱动获取发送锁失败
            ret = handle_dev_cpu_collision(skb, txq, q);
        } else {                                         // 3. 驱动发送“正忙”，当前无法发送
            ret = dev_requeue_skb(skb, q);               //    将数据重新入队，等下次发送。
        }

        if (ret && netif_xmit_frozen_or_stopped(txq))
            ret = 0;

        return ret;
```

这段代码首先释放 qdisc（发送队列）锁，然后获取（设备驱动的）发送锁。

接下来，如果发送队列没有停止，就会调用 `dev_hard_start_xmit()`。稍后将看到， 后者会把数据从 Linux 内核的网络设备子系统发送到设备驱动程序。

`dev_hard_start_xmit()` 执行之后，（或因发送队列停止而跳过执行），队列的发送锁就会被释放。

接下来，再次获取此 qdisc 的锁，然后通过调用 `dev_xmit_complete()` 检查 `dev_hard_start_xmit()` 的返回值。

1. 如果 `dev_xmit_complete()` 返回 `true`，数据已成功发送，则将 qdisc 队列长度设置为返回值，否则

2. 如果 `dev_hard_start_xmit()` 返回的是 `NETDEV_TX_LOCKED`，调用 `handle_dev_cpu_collision()` 来处理锁竞争。

   当驱动程序锁定发送队列失败时，支持 `NETIF_F_LLTX` 功能的设备会返回 `NETDEV_TX_LOCKED`。 稍后会仔细研究 `handle_dev_cpu_collision`。

现在，让我们继续关注 `sch_direct_xmit()` 并查看，以上两种情况都不满足时的情况。 如果发送失败，而且不是以上两种情况，那还有第三种可能：由于 `NETDEV_TX_BUSY`。驱动 程序返回 `NETDEV_TX_BUSY` 表示设备或驱动程序“正忙”，数据现在无法发送。这种情 况下，调用 `dev_requeue_skb()` 将数据重新入队，等下次发送。

接下来看 `handle_dev_cpu_collision()` 和 `dev_requeue_skb()`。

## 4.3 `handle_dev_cpu_collision()`

定义在 net/sched/sch_generic.c，处理两种情况：

1. 发送锁由当前 CPU 保持
2. 发送锁由其他 CPU 保存

第一种情况认为是配置问题，打印一条警告。

第二种情况，更新统计计数器 `cpu_collision`，通过 `dev_requeue_skb` 将数据重新入队 以便稍后发送。回想一下，我们在 `dequeue_skb` 中看到了专门处理重新入队的 skb 的代码。

代码很简短，可以快速阅读：

```c
static inline int handle_dev_cpu_collision(struct sk_buff *skb,
                                           struct netdev_queue *dev_queue,
                                           struct Qdisc *q)
{
        int ret;

        if (unlikely(dev_queue->xmit_lock_owner == smp_processor_id())) {
                /*
                 * Same CPU holding the lock. It may be a transient
                 * configuration error, when hard_start_xmit() recurses. We
                 * detect it by checking xmit owner and drop the packet when
                 * deadloop is detected. Return OK to try the next skb.
                 */
                kfree_skb(skb);
                net_warn_ratelimited("Dead loop on netdevice %s, fix it urgently!\n",
                                     dev_queue->dev->name);
                ret = qdisc_qlen(q);
        } else {
                /*
                 * Another cpu is holding lock, requeue & delay xmits for
                 * some time.
                 */
                __this_cpu_inc(softnet_data.cpu_collision);
                ret = dev_requeue_skb(skb, q);
        }

        return ret;
}
```

接下来看看 `dev_requeue_skb` 做了什么，后面会看到，`sch_direct_xmit` 会调用它.

## 4.4 `dev_requeue_skb()`：重新压入 qdisc 队列，等待下次发送

这个函数很简短net/sched/sch_generic.c:

```c
/* Modifications to data participating in scheduling must be protected with
 * qdisc_lock(qdisc) spinlock.
 *
 * The idea is the following:
 * - enqueue, dequeue are serialized via qdisc root lock
 * - ingress filtering is also serialized via qdisc root lock
 * - updates to tree and tree walking are only done under the rtnl mutex.
 */
static inline int dev_requeue_skb(struct sk_buff *skb, struct Qdisc *q)
{
        skb_dst_force(skb);   // skb 上强制增加一次引用计数
        q->gso_skb = skb;     // 回想一下，dequeue_skb() 中取出一个 skb 时会检查该字段
        q->qstats.requeues++; // 更新 `requeue` 计数
        q->q.qlen++;          // 更新 qdisc 队列长度

        __netif_schedule(q);  // 触发 softirq
        return 0;
}
```

接下来再回忆一遍一步步到达这里的过程，然后查看 `__netif_schedule()`。



# 5. `__qdisc_run()` 主逻辑

回想一下，我们是从 `__qdisc_run()` 开始到达这里的：

```c
void __qdisc_run(struct Qdisc *q)
{
        int quota = weight_p;
        while (qdisc_restart(q)) { // dequeue skb, send it
            if (--quota <= 0 || need_resched()) {// Ordered by possible occurrence: Postpone processing if
                    __netif_schedule(q);         // 1. we've exceeded packet quota
                    break;                       // 2. another process needs the CPU
            }                                    
        }
        qdisc_run_end(q);
}
```

`while` 循环调用 `qdisc_restart()`，后者取出一个 skb，然后尝试通过 `sch_direct_xmit()` 来发送；`sch_direct_xmit` 调用 `dev_hard_start_xmit` 来向驱动 程序进行实际发送。任何无法发送的 skb 都重新入队，将在 `NET_TX` softirq 中进行 发送。

发送过程的下一步是查看 `dev_hard_start_xmit()`，了解如何调用驱动程序来发送数据。但 在此之前，应该先查看 `__netif_schedule()` 以完全理解 `__qdisc_run()` 和 `dev_requeue_skb()` 的工作方式。

## 5.1 `__netif_schedule`

现在来看 `__netif_schedule()`， net/core/dev.c:

```c
void __netif_schedule(struct Qdisc *q)
{
    if (!test_and_set_bit(__QDISC_STATE_SCHED, &q->state))
            __netif_reschedule(q);
}
EXPORT_SYMBOL(__netif_schedule);

static inline void __netif_reschedule(struct Qdisc *q)
{
    struct softnet_data *sd;
    unsigned long flags;

    local_irq_save(flags);                  // 保存硬中断状态，并禁用硬中断（IRQ）
    sd = &__get_cpu_var(softnet_data);      // 获取当前 CPU 的 struct softnet_data 实例
    q->next_sched = NULL;
    *sd->output_queue_tailp = q;            // 将 qdisc 添加到 softnet_data 的 output 队列中
    sd->output_queue_tailp = &q->next_sched;
    raise_softirq_irqoff(NET_TX_SOFTIRQ);   // 重要步骤：触发 NET_TX_SOFTIRQ 类型软中断（softirq）
    local_irq_restore(flags);               // 恢复 IRQ 状态并重新启用硬中断
}
```

`test_and_set_bit()` 检查 `q->state` 中的 `__QDISC_STATE_SCHED` 位，如果为该位为 0，会将其置 1。 如果置位成功（意味着之前处于非 `__QDISC_STATE_SCHED` 状态），代码将调用 `__netif_reschedule()`，这个函数不长，但做的事情非常重要。

`__netif_reschedule()` 中的重要步骤是 `raise_softirq_irqoff()`，它触发一次 `NET_TX_SOFTIRQ` 类型 softirq。简单来说，可以认为 **softirqs 是以很高优先级在执行的内核线程，并代表内核处理数据**， 用于网络数据的收发处理（incoming 和 outgoing）。

`NET_TX_SOFTIRQ` softirq 有一个注册的回调函数 `net_tx_action()`，这意味着有一个内核线程将会执行 `net_tx_action()`。该线程偶尔会被暂停（pause），`raise_softirq_irqoff()` 会恢复（resume）其执行。让我们看一下 `net_tx_action()` 的作用，以便了解内核如何处理发送数据请求。

## 5.2 `net_tx_action()`

定义在 net/core/dev.c，由两个 if 组成，分别处理 executing CPU 的 **softnet_data 实例的两个 queue**：

1. completion queue
2. output queue

分别来看这两种情况，**这段代码在 softirq 上下文中作为一个独立的内核线程执行**。网络栈发送侧的**热路径中不适合执行的代码，将被延后（defer），然后由执行 net_tx_action() 的线程处理**。

## 5.3 `net_tx_action()` completion queue：待释放 skb 队列

`softnet_data` 的 completion queue 存放**等待释放的 skb**。函数 `dev_kfree_skb_irq` 可以将 skbs 添加到队列中以便稍后释放。设备驱动程序通常使用它来推迟释放已经发送成功的 skbs。驱动程序推迟释放 skb 的原因是，释放内存可能需要时间，而且有些代码（如 hardirq 处理程序） 需要尽可能快的执行并返回。

看一下 `net_tx_action` 第一段代码，该代码处理 completion queue 中等待释放的 skb：

```c
        if (sd->completion_queue) {
                struct sk_buff *clist;

                local_irq_disable();
                clist = sd->completion_queue;
                sd->completion_queue = NULL;
                local_irq_enable();

                while (clist) {
                        struct sk_buff *skb = clist;
                        clist = clist->next;
                        __kfree_skb(skb);
                }
        }
```

如果 completion queue 非空，`while` 循环将遍历这个列表并`__kfree_skb` 释放每个 skb 占 用的内存。**牢记，此代码在一个名为 softirq 的独立“线程”中运行 - 它并没有占用用户程序的系统时间（system time）**。

## 5.4 `net_tx_action` output queue：待发送 skb 队列

output queue 存储 **待发送的 skb**。如前所述，`__netif_reschedule()` 将数据添加到 output queue 中，通常从`__netif_schedule` 调用过来。

目前，我们看到 `__netif_schedule()` 函数在两个地方被调用：

1. `dev_requeue_skb()`：如果驱动程序返回 `NETDEV_TX_BUSY` 或者存在 CPU 冲突，可以调用此函数。
2. `__qdisc_run()`：一旦超出 quota 或者需要 reschedule，会调用`__netif_schedule`。

这个函数会将 qdisc 添加到 softnet_data 的 output queue 进行处理。 这里将输出队列处理代码拆分为三个块。

我们来看看第一块：

```c
    if (sd->output_queue) {       // 如果 output queue 上有 qdisc
        struct Qdisc *head;

        local_irq_disable();
        head = sd->output_queue;  // 将 head 指向第一个 qdisc
        sd->output_queue = NULL;
        sd->output_queue_tailp = &sd->output_queue; // 更新队尾指针
        local_irq_enable();
```

如果 output queue 上有 qdisc，则将 `head` 变量指向第一个 qdisc，并更新队尾指针。

接下来，一个 **while 循环开始遍历 qdsics 列表**：

```c
    while (head) {
        struct Qdisc *q = head;
        head = head->next_sched;

        spinlock_t *root_lock = qdisc_lock(q);

        if (spin_trylock(root_lock)) {                 // 非阻塞：尝试获取 qdisc root lock
            smp_mb__before_clear_bit();
            clear_bit(__QDISC_STATE_SCHED, &q->state); // 清除 q->state SCHED 状态位

            qdisc_run(q);                              // 执行 qdisc 规则，这会设置 q->state 的 RUNNING 状态位

            spin_unlock(root_lock);                    // 释放 qdisc 锁
        } else {
            if (!test_bit(__QDISC_STATE_DEACTIVATED, &q->state)) { // qdisc 还在运行
                __netif_reschedule(q);                 // 重新放入 queue，稍后继续尝试获取 root lock
            } else {                                   // qdisc 已停止运行，清除 SCHED 状态位
                smp_mb__before_clear_bit();
                clear_bit(__QDISC_STATE_SCHED, &q->state);
            }
        }
    }
```

`spin_trylock()` 获得 root lock 后，

1. 调用 `clear_bit()` 清除 qdisc 的 `__QDISC_STATE_SCHED` 状态位。
2. 然后执行 `qdisc_run()`，这会将 `__QDISC___STATE_RUNNING` 状态位置 1，并执行`__qdisc_run()`。

这里很重要。从系统调用开始的发送过程代表 applition 执行，花费的是系统时间；但接 下来它将转入 softirq 上下文中执行（这个 qdisc 的 skb 之前没有被发送出去发），花 费的是 softirq 时间。这种区分非常重要，因为这**直接影响着应用程序的 CPU 使用量监控**，尤其是发送大量数据的应用。换一种陈述方式：

1. 无论发送完成还是驱动程序返回错误，程序的系统时间都包括调用驱动程序发送数据所花的时间。
2. 如果驱动层发送失败（例如，设备忙于发送其他内容），则会将 qdisc 添加到 output queue，稍后由 softirq 线程处理。在这种情况下，将会额外花费一些 softirq（ `si`）时间在发送数据上。

因此，发送数据花费的总时间是下面二者之和：

1. **系统调用的系统时间**（sys time）
2. **NET_TX 类型的 softirq 时间**（softirq time）

如果 `spin_trylock()` 失败，则检查 qdisc 是否已经停止运行（`__QDISC_STATE_DEACTIVATED` 状态位），两种情况：

1. qdisc 未停用：调用 `__netif_reschedule()`，这会将 qdisc 放回到原 queue 中，稍后再次尝试获取 qdisc 锁。
2. qdisc 已停用：清除 `__QDISC_STATE_SCHED` 状态位。



# 6. `dev_hard_start_xmit`

至此，我们已经穿过了整个网络栈，最终来到 `dev_hard_start_xmit`。也许你是从 `sendmsg` 系统调用直接到达这里的，或者你是通过 qdisc 上的 softirq 线程处理网络数据来到这里的。`dev_hard_start_xmit` 将调用设备驱动程序来实际执行发送操作。

这个函数处理两种主要情况：

1. 已经准备好要发送的数据，或
2. 需要 segmentation offloading 的数据

先看第一种情况，要发送的数据已经准备好的情况。 net/code/dev.c：

```c
int dev_hard_start_xmit(struct sk_buff *skb, struct net_device *dev,
                        struct netdev_queue *txq)
{
        const struct net_device_ops *ops = dev->netdev_ops;
        int rc = NETDEV_TX_OK;
        unsigned int skb_len;

        if (likely(!skb->next)) {
                netdev_features_t features;

                /*
                 * If device doesn't need skb->dst, release it right now while
                 * its hot in this cpu cache
                 */
                if (dev->priv_flags & IFF_XMIT_DST_RELEASE)
                        skb_dst_drop(skb);

                features = netif_skb_features(skb);
```

代码首先获取设备的回调函数集合 `ops`，后面让驱动程序做一些发送数据的工作时会用到 。检查 `skb->next` 以确定此数据不是已分片数据的一部分，然后继续执行以下两项操作：

首先，检查设备是否设置了 `IFF_XMIT_DST_RELEASE` 标志。这个版本的内核中的任何“真实” 以太网设备都不使用此标志，但环回设备和其他一些软件设备使用。如果启用此特性，则可 以减少目标高速缓存条目上的引用计数，因为驱动程序不需要它。

接下来，`netif_skb_features` 获取设备支持的功能列表，并根据数据的协议类型（ `dev->protocol`）对特性列表进行一些修改。例如，如果设备支持此协议的校验和计算， 则将对 skb 进行相应的标记。 VLAN tag（如果已设置）也会导致功能标记被修改。

接下来，将检查 vlan 标记，如果设备无法 offload VLAN tag，将通过`__vlan_put_tag` 在软 件中执行此操作：

```c
                if (vlan_tx_tag_present(skb) &&
                    !vlan_hw_offload_capable(features, skb->vlan_proto)) {
                        skb = __vlan_put_tag(skb, skb->vlan_proto,
                                             vlan_tx_tag_get(skb));
                        if (unlikely(!skb))
                                goto out;

                        skb->vlan_tci = 0;
                }
```

然后，检查数据以确定这是不是 encapsulation （隧道封装）offload 请求，例如， [GRE](https://en.wikipedia.org/wiki/Generic_Routing_Encapsulation)。 在这种情况 下，feature flags 将被更新，以添加任何特定于设备的硬件封装功能：

```c
                /* If encapsulation offload request, verify we are testing
                 * hardware encapsulation features instead of standard
                 * features for the netdev
                 */
                if (skb->encapsulation)
                        features &= dev->hw_enc_features;
```

接下来，`netif_needs_gso` 用于确定 skb 是否需要分片。 如果需要，但设备不支持，则 `netif_needs_gso` 将返回 `true`，表示分片应在软件中进行。 在这种情况下，调用 `dev_gso_segment` 进行分片，代码将跳转到 gso 以发送数据包。我们稍后会看到 GSO 路径。

```c
                if (netif_needs_gso(skb, features)) {
                        if (unlikely(dev_gso_segment(skb, features)))
                                goto out_kfree_skb;
                        if (skb->next)
                                goto gso;
                }
```

如果数据不需要分片，则处理一些其他情况。 首先，数据是否需要顺序化？ 也就是说，如 果数据分布在多个缓冲区中，设备是否支持发送网络数据，还是首先需要将它们组合成单个 有序缓冲区？ 绝大多数网卡不需要在发送之前将数据顺序化，因此在几乎所有情况下， `skb_needs_linearize` 将为 `false` 然后被跳过。

```c
                                    else {
                        if (skb_needs_linearize(skb, features) &&
                            __skb_linearize(skb))
                                goto out_kfree_skb;
```

从接下来的一段注释我们可以了解到，下面的代码判断数据包是否仍然需要计算校验和。 如果设备不支持计算校验和，则在这里通过软件计算：

```c
                        /* If packet is not checksummed and device does not
                         * support checksumming for this protocol, complete
                         * checksumming here.
                         */
                        if (skb->ip_summed == CHECKSUM_PARTIAL) {
                                if (skb->encapsulation)
                                        skb_set_inner_transport_header(skb,
                                                skb_checksum_start_offset(skb));
                                else
                                        skb_set_transport_header(skb,
                                                skb_checksum_start_offset(skb));
                                if (!(features & NETIF_F_ALL_CSUM) &&
                                     skb_checksum_help(skb))
                                        goto out_kfree_skb;
                        }
                }
```

再往前，我们来到了 packet taps（tap 是包过滤器的安插点，例如抓包执行的地方）。 该函数中的下一个代码块将要发送的数据包传递给 tap（如果有的话）：

```c
                if (!list_empty(&ptype_all))
                        dev_queue_xmit_nit(skb, dev);
```

最终，调用驱动的 `ops` 里面的发送回调函数 `ndo_start_xmit` 将数据包传给网卡设备：

```c
                skb_len = skb->len;
                rc = ops->ndo_start_xmit(skb, dev);

                trace_net_dev_xmit(skb, rc, dev, skb_len);
                if (rc == NETDEV_TX_OK)
                        txq_trans_update(txq);
                return rc;
        }
```

`ndo_start_xmit` 的返回值表示发送成功与否，并作为这个函数的返回值被返回给更上层。 我们看到了这个返回值将如何影响上层：数据可能会被此时的 qdisc 重新入队，因此稍后尝试再次发送。

我们来看看 GSO 的 case。如果此函数的前面部分完成了分片，或者之前已经完成了分片但是上次发送失败，则会进入下面的代码：

```c
gso:
        do {
                struct sk_buff *nskb = skb->next;

                skb->next = nskb->next;
                nskb->next = NULL;

                if (!list_empty(&ptype_all))
                        dev_queue_xmit_nit(nskb, dev);

                skb_len = nskb->len;
                rc = ops->ndo_start_xmit(nskb, dev);
                trace_net_dev_xmit(nskb, rc, dev, skb_len);
                if (unlikely(rc != NETDEV_TX_OK)) {
                        if (rc & ~NETDEV_TX_MASK)
                                goto out_kfree_gso_skb;
                        nskb->next = skb->next;
                        skb->next = nskb;
                        return rc;
                }
                txq_trans_update(txq);
                if (unlikely(netif_xmit_stopped(txq) && skb->next))
                        return NETDEV_TX_BUSY;
        } while (skb->next);
```

此 `while` 循环会遍历分片生成的 skb 列表。每个数据包将被：

- 传给包过滤器（tap，如果有的话）
- 通过 `ndo_start_xmit` 传递给驱动程序进行发送

设备驱动 `ndo_start_xmit()`返回错误时，会进行一些错误处理，并将错误返回给更上层。 未发送的 skbs 可能会被重新入队以便稍后再次发送。

该函数的最后一部分做一些清理工作，在上面发生错误时释放一些资源：

```c
out_kfree_gso_skb:
        if (likely(skb->next == NULL)) {
                skb->destructor = DEV_GSO_CB(skb)->destructor;
                consume_skb(skb);
                return rc;
        }
out_kfree_skb:
        kfree_skb(skb);
out:
        return rc;
}
EXPORT_SYMBOL_GPL(dev_hard_start_xmit);
```

# 7. Monitoring qdiscs

## Using the tc command line tool

使用 `tc` 工具监控 qdisc 统计：

```
$ tc -s qdisc show dev ens33
```
![](img/1.png)

网络设备的 qdisc 统计对于监控系统发送数据包的运行状况至关重要。可以通过运行命令行工具 tc 来查看状态。 上面的示例显示了如何检查 ens33 的统计信息。

- `bytes`: The number of bytes that were pushed down to the driver for transmit.
- `pkt`: The number of packets that were pushed down to the driver for transmit.
- `dropped`: The number of packets that were dropped by the qdisc. This can happen if transmit queue length is not large enough to fit the data being queued to it.
- `overlimits`: Depends on the queuing discipline, but can be either the number of packets that could not be enqueued due to a limit being hit, and/or the number of packets which triggered a throttling event when dequeued.
- `requeues`: Number of times dev_requeue_skb has been called to requeue an skb. Note that an skb which is requeued multiple times will bump this counter each time it is requeued.
- `backlog`: Number of bytes currently on the qdisc’s queue. This number is usually bumped each time a packet is enqueued.

一些 qdisc 还会导出额外的统计信息。每个 qdisc 都不同，对同一个 counter 可能会累积不同的次数。需要查看相应 qdisc 的源代码，弄清楚每个 counter 是在哪里、什么条件下被更新的。



# 8. Tuning qdiscs

## 8.1 调整`__qdisc_run` 处理权重

可以调整前面看到的`__qdisc_run` 循环的权重（上面看到的 `quota` 变量），这将导致 `__netif_schedule` 更多的被调用执行。 结果将是当前 qdisc 将被更多的添加到当前 CPU 的 `output_queue`，最终会使发包所占的时间变多。

例如：调整所有 qdisc 的`__qdisc_run` 权重：

```
$ sudo sysctl -w net.core.dev_weight=600
```

## 8.2 增加发送队列长度

每个网络设备都有一个可以修改的 txqueuelen。 大多数 qdisc 在将数据插入到其发送队列之前，会检查 txqueuelen 是否足够。 可以调整这个参数以增加 qdisc 队列的字节数。

Example: increase the `txqueuelen` of `ens33` to `10000`.

```
$ sudo ifconfig ens33 txqueuelen 10000
```

默认值是 1000，可以通过 ifconfig 命令的输出，查看每个网络设备的 txqueuelen。

参考资料：https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data
