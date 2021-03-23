---
title: "Linux内核网络数据包发送（四）——Linux netdevice 子系统"
date: 2020-11-02T10:07:28+08:00
author: "梁金荣"
keywords: ["内核网络"]
categories : ["内核网络"]
banner : "img/blogimg/ljrimg15.jpg"
summary : "本文主要分析 dev_queue_xmit 发送数据包的过程，并进行调优。"
---

# 1. 前言
在分析 `dev_queue_xmit` 发送数据包之前，我们需要了解以下重要概念。

Linux 支持流量控制（traffic control）的功能，此功能允许系统管理员控制数据包如何从机器发送出去。流量控制系统包含几组不同的 queue system，每种有不同的排队特征。各个排队系统通常称为 qdisc，也称为排队规则。可以将 qdisc 视为**调度程序**， qdisc 决定数据包的发送时间和方式。

Linux 上每个 device 都有一个与之关联的默认 qdisc。对于仅支持单发送队列的网卡，使用默认的 qdisc `pfifo_fast`。支持多个发送队列的网卡使用 mq 的默认 qdisc。可以运行 `tc qdisc` 来查看系统 qdisc 信息。某些设备支持硬件流量控制，这允许管理员将流量控制 offload 到网络硬件，节省系统的 CPU 资源。

现在我们从 net/core/dev.c 继续分析 `dev_queue_xmit`。

# 2. `dev_queue_xmit` and `__dev_queue_xmit`

`dev_queue_xmit` 简单封装了`__dev_queue_xmit`:

```c
int dev_queue_xmit(struct sk_buff *skb)
{
        return __dev_queue_xmit(skb, NULL);
}
EXPORT_SYMBOL(dev_queue_xmit);
```

`__dev_queue_xmit` 才是干脏活累活的地方，我们一点一点来看：

```c
static int __dev_queue_xmit(struct sk_buff *skb, void *accel_priv)
{
        struct net_device *dev = skb->dev;
        struct netdev_queue *txq;
        struct Qdisc *q;
        int rc = -ENOMEM;

        skb_reset_mac_header(skb);

        /* Disable soft irqs for various locks below. Also
         * stops preemption for RCU.
         */
        rcu_read_lock_bh();

        skb_update_prio(skb);
```

开始的逻辑：

1. 声明变量
2. 调用 `skb_reset_mac_header`，准备发送 skb。这会重置 skb 内部的指针，使得 ether 头可以被访问
3. 调用 `rcu_read_lock_bh`，为接下来的读操作加锁
4. 调用 `skb_update_prio`，如果启用了网络优先级 cgroups，这会设置 skb 的优先级

现在，我们来看更复杂的部分：

```c
        txq = netdev_pick_tx(dev, skb, accel_priv);
```

这会选择发送队列。

## 2.1 `netdev_pick_tx`

`netdev_pick_tx` 定义在net/core/flow_dissector.c

```c
struct netdev_queue *netdev_pick_tx(struct net_device *dev,
                                    struct sk_buff *skb,
                                    void *accel_priv)
{
        int queue_index = 0;

        if (dev->real_num_tx_queues != 1) {
                const struct net_device_ops *ops = dev->netdev_ops;
                if (ops->ndo_select_queue)
                        queue_index = ops->ndo_select_queue(dev, skb,
                                                            accel_priv);
                else
                        queue_index = __netdev_pick_tx(dev, skb);

                if (!accel_priv)
                        queue_index = dev_cap_txqueue(dev, queue_index);
        }

        skb_set_queue_mapping(skb, queue_index);
        return netdev_get_tx_queue(dev, queue_index);
}
```

如上所示，如果网络设备仅支持单个 TX 队列，则会跳过复杂的代码，直接返回单个 TX 队列。 大多高端服务器上使用的设备都有多个 TX 队列。具有多个 TX 队列的设备有两种情况：

1. 驱动程序实现 `ndo_select_queue`，以硬件或 feature-specific 的方式更智能地选择 TX 队列
2. 驱动程序没有实现 `ndo_select_queue`，这种情况需要内核自己选择设备

从 3.13 内核开始，没有多少驱动程序实现 `ndo_select_queue`。bnx2x 和 ixgbe 驱动程序实现了此功能，但仅用于以太网光纤通道FCoE。鉴于此，我们假设网络设备没有实现 `ndo_select_queue` 和没有使用 FCoE。在这种情况下，内核将使用`__netdev_pick_tx` 选择 tx 队列。

一旦`__netdev_pick_tx` 确定了队列号，`skb_set_queue_mapping` 将缓存该值（稍后将在流量控制代码中使用），`netdev_get_tx_queue` 将查找并返回指向该队列的指针。让我们 看一下`__netdev_pick_tx` 在返回`__dev_queue_xmit` 之前的工作原理。

## 2.2 `__netdev_pick_tx`

我们来看内核如何选择 TX 队列。 net/core/flow_dissector.c:

```c
u16 __netdev_pick_tx(struct net_device *dev, struct sk_buff *skb)
{
        struct sock *sk = skb->sk;
        int queue_index = sk_tx_queue_get(sk);

        if (queue_index < 0 || skb->ooo_okay ||
            queue_index >= dev->real_num_tx_queues) {
                int new_index = get_xps_queue(dev, skb);
                if (new_index < 0)
                        new_index = skb_tx_hash(dev, skb);

                if (queue_index != new_index && sk &&
                    rcu_access_pointer(sk->sk_dst_cache))
                        sk_tx_queue_set(sk, new_index);

                queue_index = new_index;
        }

        return queue_index;
}
```

代码首先调用 `sk_tx_queue_get` 检查发送队列是否已经缓存在 socket 上，如果尚未缓存， 则返回-1。

下一个 if 语句检查是否满足以下任一条件：

1. `queue_index < 0`：表示尚未设置 TX queue 的情况
2. `ooo_okay` 标志是否非零：如果不为 0，则表示现在允许无序（out of order）数据包。 协议层必须正确地设置此标志。当 flow 的所有 outstanding（需要确认的）数据包都已确认时，TCP 协议层将设置此标志。当发生这种情况时，内核可以为此数据包选择不同的 TX 队列。UDP 协议层不设置此标志 ，因此 UDP 数据包永远不会将 `ooo_okay` 设置为非零值。
3. TX queue index 大于 TX queue 数量：如果用户最近通过 ethtool 更改了设备上的队列数， 则会发生这种情况。

以上任何一种情况，都表示没有找到合适的 TX queue，因此接下来代码会进入慢路径以继续寻找合适的发送队列。首先调用 `get_xps_queue`，它会使用一个由用户配置的 TX queue 到 CPU 的映射，这称为 XPS（Transmit Packet Steering ，发送数据包控制）。

如果内核不支持 XPS，或者系统管理员未配置 XPS，或者配置的映射引用了无效队列， `get_xps_queue` 返回-1，则代码将继续调用 `skb_tx_hash`。

一旦 XPS 或内核使用 `skb_tx_hash` 自动选择了发送队列，`sk_tx_queue_set` 会将队列缓存 在 socket 对象上，然后返回。让我们看看 XPS，以及 `skb_tx_hash` 在继续调用 `dev_queue_xmit` 之前是如何工作的。

### 2.2.1 Transmit Packet Steering (XPS)

发送数据包控制（XPS）是一项功能，允许系统管理员配置哪些 CPU 可以处理网卡的哪些发送 队列。XPS 的主要目的是**避免处理发送请求时的锁竞争**。使用 XPS 还可以减少缓存驱逐， 避免NUMA机器上的远程内存访问等。

上面代码中，`get_xps_queue` 将查询这个用户指定的映射，以确定应使用哪个发送 队列。如果 `get_xps_queue` 返回-1，则将改为使用 `skb_tx_hash`。

### 2.2.2 `skb_tx_hash`

如果 XPS 未包含在内核中，或 XPS 未配置，或配置的队列不可用（可能因为用户调整了队列数 ），`skb_tx_hash` 将接管以确定应在哪个队列上发送数据。准确理解 `skb_tx_hash` 的工作原理非常重要，具体取决于你的发送负载。include/linux/netdevice.h：

```c
/*
 * Returns a Tx hash for the given packet when dev->real_num_tx_queues is used
 * as a distribution range limit for the returned value.
 */
static inline u16 skb_tx_hash(const struct net_device *dev,
                              const struct sk_buff *skb)
{
        return __skb_tx_hash(dev, skb, dev->real_num_tx_queues);
}
```

直接调用了` __skb_tx_hash`, net/core/flow_dissector.c：

```c
/*
 * Returns a Tx hash based on the given packet descriptor a Tx queues' number
 * to be used as a distribution range.
 */
u16 __skb_tx_hash(const struct net_device *dev, const struct sk_buff *skb,
                  unsigned int num_tx_queues)
{
        u32 hash;
        u16 qoffset = 0;
        u16 qcount = num_tx_queues;

        if (skb_rx_queue_recorded(skb)) {
                hash = skb_get_rx_queue(skb);
                while (unlikely(hash >= num_tx_queues))
                        hash -= num_tx_queues;
                return hash;
        }
```

这个函数中的第一个 if 是一个有趣的短路，函数名 `skb_rx_queue_recorded` 有点误导。skb 有一个 `queue_mapping` 字段，rx 和 tx 都会用到这个字段。无论如何，如果系统正在接收数据包并将其转发到其他地方，则此 if 语句都为 `true`。否则，代码将继续向下：

```c
        if (dev->num_tc) {
                u8 tc = netdev_get_prio_tc_map(dev, skb->priority);
                qoffset = dev->tc_to_txq[tc].offset;
                qcount = dev->tc_to_txq[tc].count;
        }
```

要理解这段代码，首先要知道，程序可以设置 socket 上发送的数据的优先级。这可以通过 `setsockopt` 带 `SOL_SOCKET` 和 `SO_PRIORITY` 选项来完成。

如果使用 `setsockopt` 带 `IP_TOS` 选项来设置在 socket 上发送的 IP 包的 TOS 标志（ 或者作为辅助消息传递给 `sendmsg`，在数据包级别设置），内核会将其转换为 `skb->priority`。

如前所述，一些网络设备支持基于硬件的流量控制系统。**如果 num_tc 不为零，则表示此设 备支持基于硬件的流量控制**。这种情况下，将查询一个**packet priority 到该硬件支持 的流量控制**的映射，根据此映射选择适当的流量类型（traffic class）。

接下来，将计算出该 traffic class 的 TX queue 的范围，它将用于确定发送队列。如果 `num_tc` 为零（网络设备不支持硬件流量控制），则 `qcount` 和 `qoffset` 变量分 别设置为发送队列数和 0。

使用 `qcount` 和 `qoffset`，将计算发送队列的 index：

```c
        if (skb->sk && skb->sk->sk_hash)
                hash = skb->sk->sk_hash;
        else
                hash = (__force u16) skb->protocol;
        hash = __flow_hash_1word(hash);

        return (u16) (((u64) hash * qcount) >> 32) + qoffset;
}
EXPORT_SYMBOL(__skb_tx_hash);
```

最后，通过`__netdev_pick_tx` 返回选出的 TX queue index。



# 3. 继续`__dev_queue_xmit`
至此已经选到了合适的发送队列，继续`__dev_queue_xmit `:

```c
        q = rcu_dereference_bh(txq->qdisc);

#ifdef CONFIG_NET_CLS_ACT
        skb->tc_verd = SET_TC_AT(skb->tc_verd, AT_EGRESS);
#endif
        trace_net_dev_queue(skb);
        if (q->enqueue) {
                rc = __dev_xmit_skb(skb, q, dev, txq);
                goto out;
        }
```

首先获取与此队列关联的 qdisc。之前我们看到单发送队列设备的默认类型是 `pfifo_fast` qdisc，而对于多队列设备，默认类型是 `mq` qdisc。

接下来，如果内核中已启用数据包分类 API，则代码会为 packet 分配 traffic class。 接下来，检查 disc 是否有合适的队列来存放 packet。像 `noqueue` 这样的 qdisc 没有队列。 如果有队列，则代码调用`__dev_xmit_skb` 继续处理数据，然后跳转到此函数的末尾。我们很快 就会看到`__dev_xmit_skb`。现在，让我们看看如果没有队列会发生什么，从一个非常有用的注释开始：

```c
        /* The device has no queue. Common case for software devices:
           loopback, all the sorts of tunnels...

           Really, it is unlikely that netif_tx_lock protection is necessary
           here.  (f.e. loopback and IP tunnels are clean ignoring statistics
           counters.)
           However, it is possible, that they rely on protection
           made by us here.

           Check this and shot the lock. It is not prone from deadlocks.
           Either shot noqueue qdisc, it is even simpler 8)
         */
        if (dev->flags & IFF_UP) {
                int cpu = smp_processor_id(); /* ok because BHs are off */
```

正如注释所示，**唯一可以拥有”没有队列的 qdisc”的设备是环回设备和隧道设备**。如果设备当前处于运行状态，则获取当前 CPU，然后判断此设备队列上的发送锁是否由此 CPU 拥有 ：

```c
                if (txq->xmit_lock_owner != cpu) {

                        if (__this_cpu_read(xmit_recursion) > RECURSION_LIMIT)
                                goto recursion_alert;
```

如果发送锁不由此 CPU 拥有，则在此处检查 per-CPU 计数器变量 `xmit_recursion`，判断其是 否超过 `RECURSION_LIMIT`。 一个程序可能会在这段代码这里持续发送数据，然后被抢占， 调度程序选择另一个程序来运行。第二个程序也可能驻留在此持续发送数据。因此， `xmit_recursion` 计数器用于确保在此处竞争发送数据的程序不超过 `RECURSION_LIMIT` 个 。

我们继续：

```c
                        HARD_TX_LOCK(dev, txq, cpu);

                        if (!netif_xmit_stopped(txq)) {
                                __this_cpu_inc(xmit_recursion);
                                rc = dev_hard_start_xmit(skb, dev, txq);
                                __this_cpu_dec(xmit_recursion);
                                if (dev_xmit_complete(rc)) {
                                        HARD_TX_UNLOCK(dev, txq);
                                        goto out;
                                }
                        }
                        HARD_TX_UNLOCK(dev, txq);
                        net_crit_ratelimited("Virtual device %s asks to queue packet!\n",
                                             dev->name);
                } else {
                        /* Recursion is detected! It is possible,
                         * unfortunately
                         */
recursion_alert:
                        net_crit_ratelimited("Dead loop on virtual device %s, fix it urgently!\n",
                                             dev->name);
                }
        }
```

接下来的代码首先尝试获取发送锁，然后检查要使用的设备的发送队列是否被停用。如果没有停用，则更新 `xmit_recursion` 计数，然后将数据向下传递到更靠近发送的设备。或者，如果当前 CPU 是发送锁定的拥有者，或者如果 `RECURSION_LIMIT` 被命中，则不进行发送，而会打印告警日志。函数剩余部分的代码设置错误码并返回。

由于我们对真正的以太网设备感兴趣，让我们来看一下之前就需要跟进去的 `__dev_xmit_skb` 函数，这是发送主线上的函数。



# 4.  `__dev_xmit_skb`

现在我们带着排队规则 `qdisc`、网络设备 `dev` 和发送队列 `txq` 三个变量来到 `__dev_xmit_skb`，net/core/dev.c：

```c
static inline int __dev_xmit_skb(struct sk_buff *skb, struct Qdisc *q,
                                 struct net_device *dev,
                                 struct netdev_queue *txq)
{
        spinlock_t *root_lock = qdisc_lock(q);
        bool contended;
        int rc;

        qdisc_pkt_len_init(skb);
        qdisc_calculate_pkt_len(skb, q);
        /*
         * Heuristic to force contended enqueues to serialize on a
         * separate lock before trying to get qdisc main lock.
         * This permits __QDISC_STATE_RUNNING owner to get the lock more often
         * and dequeue packets faster.
         */
        contended = qdisc_is_running(q);
        if (unlikely(contended))
                spin_lock(&q->busylock);
```

代码首先使用 `qdisc_pkt_len_init` 和 `qdisc_calculate_pkt_len` 来计算数据的准确长度 ，稍后 qdisc 会用到该值。 对于硬件 offload（例如 UFO）这是必需的，因为添加的额外的头 信息，硬件 offload 的时候回用到。

接下来，使用另一个锁来帮助减少 qdisc 主锁上的竞争（我们稍后会看到这第二个锁）。 如 果 qdisc 当前正在运行，那么试图发送的其他程序将在 qdisc 的 `busylock` 上竞争。 这允许 运行 qdisc 的程序在处理数据包的同时，与较少量的程序竞争第二个主锁。随着竞争者数量 的减少，这种技巧增加了吞吐量。 接下来是主锁：

```c
        spin_lock(root_lock);
```

接下来处理 3 种可能情况：

1. 如果 qdisc 已停用
2. 如果 qdisc 允许数据包 bypass 排队系统，并且没有其他包要发送，并且 qdisc 当前没有运 行。允许包 bypass 所谓的 **work-conserving qdisc 那些用于流量整形（traffic reshaping）目的并且不会引起发送延迟的 qdisc**
3. 所有其他情况

让我们来看看每种情况下发生什么，从 qdisc 停用开始：

```c
        if (unlikely(test_bit(__QDISC_STATE_DEACTIVATED, &q->state))) {
                kfree_skb(skb);
                rc = NET_XMIT_DROP;
```
 如果 qdisc 停用，则释放数据并将返回代码设置为 `NET_XMIT_DROP`。接下来，如果 qdisc 允许数据包 bypass，并且没有其他包要发送，并且 qdisc 当前没有运行：

```c
        } else if ((q->flags & TCQ_F_CAN_BYPASS) && !qdisc_qlen(q) &&
                   qdisc_run_begin(q)) {
                /*
                 * This is a work-conserving queue; there are no old skbs
                 * waiting to be sent out; and the qdisc is not running -
                 * xmit the skb directly.
                 */
                if (!(dev->priv_flags & IFF_XMIT_DST_RELEASE))
                        skb_dst_force(skb);

                qdisc_bstats_update(q, skb);

                if (sch_direct_xmit(skb, q, dev, txq, root_lock)) {
                        if (unlikely(contended)) {
                                spin_unlock(&q->busylock);
                                contended = false;
                        }
                        __qdisc_run(q);
                } else
                        qdisc_run_end(q);

                rc = NET_XMIT_SUCCESS;
```

这个 if 语句有点复杂，如果满足以下所有条件，则整个语句的计算结果为 true：

1. `q-> flags＆TCQ_F_CAN_BYPASS`：qdisc 允许数据包绕过排队系统。对于所谓的“ work-conserving” qdiscs 这会是 `true`；即，允许 packet bypass 流量整形 qdisc。 `pfifo_fast` qdisc 允许数据包 bypass
2. `!qdisc_qlen(q)`：qdisc 的队列中没有待发送的数据
3. `qdisc_run_begin(p)`：如果 qdisc 未运行，此函数将设置 qdisc 的状态为“running”并返 回 `true`，如果 qdisc 已在运行，则返回 `false`

如果以上三个条件都为 `true`，那么：

- 检查 `IFF_XMIT_DST_RELEASE` 标志，此标志允许内核释放 skb 的目标缓存。如果标志已禁用，将强制对 skb 进行引用计数
- 调用 `qdisc_bstats_update` 更新 qdisc 发送的字节数和包数统计
- 调用 `sch_direct_xmit` 用于发送数据包。我们将很快深入研究 `sch_direct_xmit`，因为慢路径也会调用到它

`sch_direct_xmit` 的返回值有两种情况：

1. 队列不为空（返回> 0）。在这种情况下，`busylock` 将被释放，然后调用`__qdisc_run` 重新启动 qdisc 处理
2. 队列为空（返回 0）。在这种情况下，`qdisc_run_end` 用于关闭 qdisc 处理

在任何一种情况下，都会返回 `NET_XMIT_SUCCESS`。

让我们检查最后一种情况：

```c
        } else {
                skb_dst_force(skb);
                rc = q->enqueue(skb, q) & NET_XMIT_MASK;
                if (qdisc_run_begin(q)) {
                        if (unlikely(contended)) {
                                spin_unlock(&q->busylock);
                                contended = false;
                        }
                        __qdisc_run(q);
                }
        }
```

在所有其他情况下：

1. 调用 `skb_dst_force` 强制对 skb 的目标缓存进行引用计数
2. 调用 qdisc 的 `enqueue` 方法将数据入队，保存函数返回值
3. 调用 `qdisc_run_begin(p)`将 qdisc 标记为正在运行。如果它尚未运行（`contended == false`），则释放 `busylock`，然后调用`__qdisc_run(p)`启动 qdisc 处理

函数最后释放相应的锁，并返回状态码：

```c
        spin_unlock(root_lock);
        if (unlikely(contended))
                spin_unlock(&q->busylock);
        return rc;
```



# 5. 调优: Transmit Packet Steering (XPS)

使用 XPS 需要在内核配置中启用它，并提供一个位掩码，用于 描述**CPU 和 TX queue 的对应关系**，这些位掩码类似于 RPS位掩码，简而言之，要修改的位掩码位于以下位置：

```
/sys/class/net/DEVICE_NAME/queues/QUEUE/xps_cpus
```

因此，对于 eth0 和 TX queue 0，需要使用十六进制数修改文件： `/sys/class/net/eth0/queues/tx-0/xps_cpus`，制定哪些 CPU 应处理来自 eth0 的发送队列 0 的发送过程。另外，内核文档Documentation/networking/scaling.txt#L412-L422 指出，在某些配置中可能不需要 XPS。

参考资料：https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data
