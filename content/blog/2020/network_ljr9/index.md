---
title: "内核网络中的GRO、RFS、RPS技术介绍和调优"
date: 2020-09-15T17:07:28+08:00
author: "梁金荣"
keywords: ["内核网络"]
categories : ["内核网络"]
banner : "img/blogimg/ljrimg6.jpg"
summary : "本文主要介绍内核网络中GRO、RFS、RPS等技术，并针对其对应的规则进行网络调优。重点对RPS的工作过程和内核代码进行了分析，分析了数据如何从网卡进入到协议层。"
---

## 1. 前言
本文主要介绍内核网络中GRO、RFS、RPS等技术，并针对其对应的规则进行网络调优。重点对RPS的工作过程和内核代码进行了分析，分析了数据如何从网卡进入到协议层。
## 2. GRO（Generic Receive Offloading）
Large Receive Offloading (LRO) 是一个硬件优化，GRO 是 LRO 的一种软件实现。

两种方案的主要思想都是：通过合并“足够类似”的包来减少传送给网络栈的包数，这有助于减少 CPU 的使用量。例如，考虑大文件传输的场景，包的数量非常多，大部分包都是一段文件数据。相比于每次都将小包送到网络栈，可以将收到的小包合并成一个很大的包再送到网络栈。GRO 使协议层只需处理一个 header，而将包含大量数据的整个大包送到用户程序。

这类优化方式的缺点是信息丢失：包的 option 或者 flag 信息在合并时会丢失。这也是为什么大部分人不使用或不推荐使用LRO 的原因。

LRO 的实现，一般来说，对合并包的规则非常宽松。GRO 是 LRO 的软件实现，但是对于包合并 的规则更严苛。如果用 tcpdump 抓包，有时会看到机器收到了看起来不现实的、非常大的包， 这很可能是系统开启了 GRO。接下来会看到，tcpdump 的抓包点（捕获包的 tap ）在GRO 之后。

### 2.1 使用 ethtool 修改 GRO 配置
使用 ethtool 的 -k 选项查看 GRO 配置：

![](img/1.png)

-K 修改 GRO 配置：
```bash
$ sudo ethtool -K ens33 gro on
```
注意：对于大部分驱动，修改 GRO 配置会涉及先 down 再 up 这个网卡，因此这个网卡上的连接 都会中断。

### 2.2 napi_gro_receive
如果开启了 GRO，`napi_gro_receive` 将负责处理网络数据，并将数据送到协议栈，大部分相关的逻辑在函数 `dev_gro_receive` 里实现。

 `dev_gro_receive`这个函数首先检查 GRO 是否开启了，如果是，就准备做 GRO。GRO 首先遍历一个 `offload filter` 列表，如果高层协议认为其中一些数据属于 GRO 处理的范围，就会允许其对数据进行操作。

协议层以此方式让网络设备层知道，这个 packet 是不是当前正在处理的一个需要做 GRO 的 `network flow` 的一部分，而且也可以通过这种方式传递一些协议相关的信息。例如，TCP 协议需要判断是否应该将一个 ACK 包合并到其他包里。net/core/dev.c:
```c
list_for_each_entry_rcu(ptype, head, list) {
	if (ptype->type != type || !ptype->callbacks.gro_receive)
		continue;

	skb_set_network_header(skb, skb_gro_offset(skb));
	skb_reset_mac_len(skb);
	NAPI_GRO_CB(skb)->same_flow = 0;
	NAPI_GRO_CB(skb)->flush = 0;
	NAPI_GRO_CB(skb)->free = 0;
	
	pp = ptype->callbacks.gro_receive(&napi->gro_list, skb);
	break;
}
```
如果协议层提示是时候 `flush GRO packet` 了，那就到下一步处理了。这发生在 `napi_gro_complete`，会进一步调用相应协议的 `gro_complete` 回调方法，然后调用 `netif_receive_skb` 将包送到协议栈。 这个过程见net/core/dev.c：
```c
if (pp) {
	struct sk_buff *nskb = *pp;
	
	*pp = nskb->next;
	nskb->next = NULL;
	napi_gro_complete(nskb);
	napi->gro_count--;
}
```
接下来，如果协议层将这个包合并到一个已经存在的 flow，`napi_gro_receive` 就没什么事情需要做，因此就返回了。如果 packet 没有被合并，而且 GRO 的数量小于 `MAX_GRO_SKBS`（ 默认是 8），就会创建一个新的 entry 加到本 CPU 的 NAPI 变量的 `gro_list`。 net/core/dev.c：
```c
if (NAPI_GRO_CB(skb)->flush || napi->gro_count >= MAX_GRO_SKBS)
	goto normal;

napi->gro_count++;
NAPI_GRO_CB(skb)->count = 1;
NAPI_GRO_CB(skb)->age = jiffies;
skb_shinfo(skb)->gso_size = skb_gro_len(skb);
skb->next = napi->gro_list;
napi->gro_list = skb;
ret = GRO_HELD;
```
这就是 Linux 网络栈中 GRO 的工作原理。


### 2.3 `napi_skb_finish`
一旦 `dev_gro_receive` 完成，`napi_skb_finish` 就会被调用，其如果一个 packet 被合并了 ，就释放不用的变量；或者调用 `netif_receive_skb` 将数据发送到网络协议栈。

## 3. RFS (Receive Flow Steering)
RFS（Receive flow steering）和 RPS 配合使用。RPS 试图在 CPU 之间平衡收包，但是没考虑数据的本地性问题，如何最大化 CPU 缓存的命中率。RFS 将属于相同 flow 的包送到相同的 CPU 进行处理，可以提高缓存命中率。

### 调优：打开 RFS
RPS 记录一个全局的 `hash table`，包含所有 flow 的信息。这个 hash table 的大小可以在 `net.core.rps_sock_flow_entries`配置：
```bash
$ sudo sysctl -w net.core.rps_sock_flow_entries=32768
```
其次，可以设置每个 RX queue 的 flow 数量，对应着 `rps_flow_cnt`：

例如，eth0 的 RX queue0 的 flow 数量调整到 2048：
```bash
$ sudo bash -c 'echo 2048 > /sys/class/net/eth0/queues/rx-0/rps_flow_cnt'
```
## 4. RPS（Receive Packet Steering）
每个 NAPI 变量都会运行在相应 CPU 的软中断的上下文中。而且，触发硬中断的这个 CPU 接下来会负责执行相应的软中断处理函数来收包。换言之，同一个 CPU 既处理硬中断，又处理相应的软中断。

一些网卡（例如 Intel I350）在硬件层支持多队列。这意味着收进来的包会被通过 DMA 放到位于不同内存的队列上，而不同的队列有相应的 NAPI 变量管理软中断 poll()过程。因此， 多个 CPU 同时处理从网卡来的中断，处理收包过程。这个特性被称作 RSS（Receive Side Scaling，接收端扩展）。

RPS （Receive Packet Steering，接收包控制，接收包引导）是 RSS 的一种软件实现。因为是软件实现的，意味着任何网卡都可以使用这个功能，即便是那些只有一个接收队列的网卡。但是，因为它是软件实现的，这意味着 RPS 只能在 packet 通过 DMA 进入内存后，RPS 才能开始工作。

这意味着，RPS 并不会减少 CPU 处理硬件中断和 NAPI poll（软中断最重要的一部分）的时间，但是可以在 packet 到达内存后，将 packet 分到其他 CPU，从其他 CPU 进入协议栈。

### 4.1 不使用 RPS（默认）
如果 RPS 没启用，会调用`__netif_receive_skb`，它做一些 bookkeeping 工作，进而调用 `__netif_receive_skb_core`，将数据移动到离协议栈更近一步。

### 4.2 使用 RPS
如果 RPS 启用了，它会做一些计算，判断使用哪个 CPU 的 `backlog queue`，这个过程由 `get_rps_cpu` 函数完成。 net/core/dev.c:
```c
cpu = get_rps_cpu(skb->dev, skb, &rflow);

if (cpu >= 0) {
	ret = enqueue_to_backlog(skb, cpu, &rflow->last_qtail);
	rcu_read_unlock();
	return ret;
}
```
`get_rps_cpu` 会考虑 RFS 和 aRFS 设置，以此选出一个合适的 CPU，通过调用 `enqueue_to_backlog` 将数据放到它的 `backlog queue`。
>假如你的网卡支持 aRFS，你可以开启它并做如下配置：
>- 打开并配置 RPS
>- 打开并配置 RFS
>- 内核中编译期间指定了 CONFIG_RFS_ACCEL 选项。Ubuntu kernel 3.13.0 是有的
>- 打开网卡的 ntuple 支持。可以用 ethtool 查看当前的 ntuple 设置
>- 配置 IRQ（硬中断）中每个 RX 和 CPU 的对应关系

>以上配置完成后，aRFS 就会自动将 RX queue 数据移动到指定 CPU 的内存，每个 flow 的包都会到达同一个 CPU，不需要你再通过 ntuple 手动指定每个 flow 的配置了。

#### 4.2.1 enqueue_to_backlog
首先从远端 CPU 的 `struct softnet_data` 变量获取 `backlog queue` 长度。如果 backlog 大于 `netdev_max_backlog`，或者超过了 `flow limit`，直接 drop，并更新 `softnet_data` 的 drop 统计。注意这是远端 CPU 的统计。net/core/dev.c:
```c
    qlen = skb_queue_len(&sd->input_pkt_queue);
    if (qlen <= netdev_max_backlog && !skb_flow_limit(skb, qlen)) {
    	if (skb_queue_len(&sd->input_pkt_queue)) {
enqueue:
            __skb_queue_tail(&sd->input_pkt_queue, skb);
            input_queue_tail_incr_save(sd, qtail);
            return NET_RX_SUCCESS;
        }

        /* Schedule NAPI for backlog device */
        if (!__test_and_set_bit(NAPI_STATE_SCHED, &sd->backlog.state)) {
            if (!rps_ipi_queued(sd))
                ____napi_schedule(sd, &sd->backlog);
        }
        goto enqueue;
    }
    sd->dropped++;

    kfree_skb(skb);
    return NET_RX_DROP;
```
`enqueue_to_backlog` 被调用的地方很少。在基于 RPS 处理包的地方，以及 `netif_rx`，会调用到它。大部分驱动都不应该使用 `netif_rx`，而应该是用 `netif_receive_skb`。如果你没用到 RPS，你的驱动也没有使用 `netif_rx`，那增大 backlog 并不会带来益处，因为它根本没被用到。

注意：检查驱动，如果它调用了 `netif_receive_skb`，而且没用 RPS，那增大 `netdev_max_backlog` 并不会带来任何性能提升，因为没有数据包会被送到 `input_pkt_queue`。

如果 `input_pkt_queue` 足够小，而 flow limit 也还没达到（或者被禁掉了 ），那数据包将会被放到队列。这里的逻辑有点 funny，但大致可以归为为：

- 如果 backlog 是空的：如果远端 CPU NAPI 变量没有运行，并且 IPI 没有被加到队列，那就 触发一个 IPI 加到队列，然后调用`____napi_schedule` 进一步处理。
- 如果 backlog 非空，或者远端 CPU NAPI 变量正在运行，那就 enqueue 包
这里使用了 goto，所以代码看起来有点 tricky。

net/core/dev.c:
```c
  if (skb_queue_len(&sd->input_pkt_queue)) {
enqueue:
         __skb_queue_tail(&sd->input_pkt_queue, skb);
         input_queue_tail_incr_save(sd, qtail);
         rps_unlock(sd);
         local_irq_restore(flags);
         return NET_RX_SUCCESS;
 }

 /* Schedule NAPI for backlog device
  * We can use non atomic operation since we own the queue lock
  */
 if (!__test_and_set_bit(NAPI_STATE_SCHED, &sd->backlog.state)) {
         if (!rps_ipi_queued(sd))
                 ____napi_schedule(sd, &sd->backlog);
 }
 goto enqueue;
```
#### 4.2.2 Flow limits
RPS 在不同 CPU 之间分发 packet，但是，如果一个 flow 特别大，会出现单个 CPU 被打爆，而其他 CPU 无事可做（饥饿）的状态。因此引入了 flow limit 特性，放到一个 backlog 队列的属 于同一个 flow 的包的数量不能超过一个阈值。这可以保证即使有一个很大的 flow 在大量收包 ，小 flow 也能得到及时的处理。net/core/dev.c：
```c
/*
 * enqueue_to_backlog is called to queue an skb to a per CPU backlog
 * queue (may be a remote CPU queue).
 */
static int enqueue_to_backlog(struct sk_buff *skb, int cpu,
			      unsigned int *qtail)
{
	struct softnet_data *sd;
	unsigned long flags;
	unsigned int qlen;

	sd = &per_cpu(softnet_data, cpu);

	local_irq_save(flags);

	rps_lock(sd);
	qlen = skb_queue_len(&sd->input_pkt_queue);
	if (qlen <= netdev_max_backlog && !skb_flow_limit(skb, qlen)) {
		if (skb_queue_len(&sd->input_pkt_queue)) {
enqueue:
			__skb_queue_tail(&sd->input_pkt_queue, skb);
			input_queue_tail_incr_save(sd, qtail);
			rps_unlock(sd);
			local_irq_restore(flags);
			return NET_RX_SUCCESS;
		}

		/* Schedule NAPI for backlog device
		 * We can use non atomic operation since we own the queue lock
		 */
		if (!__test_and_set_bit(NAPI_STATE_SCHED, &sd->backlog.state)) {
			if (!rps_ipi_queued(sd))
				____napi_schedule(sd, &sd->backlog);
		}
		goto enqueue;
	}

	sd->dropped++;
	rps_unlock(sd);

	local_irq_restore(flags);

	atomic_long_inc(&skb->dev->rx_dropped);
	kfree_skb(skb);
	return NET_RX_DROP;
}
```
默认，flow limit 功能是关掉的。要打开 flow limit，需要指定一个 bitmap（类似于 RPS 的 bitmap）。

**监控**：由于 `input_pkt_queue` 打满或 flow limit 导致的丢包，在`/proc/net/softnet_stat` 里面的 dropped 列计数。

**调优**
Tuning: Adjusting netdev_max_backlog to prevent drops
在调整这个值之前，请先阅读前面的“注意”。

如果使用了 RPS，或者驱动调用了 `netif_rx`，那增加 `netdev_max_backlog` 可以改善在 `enqueue_to_backlog` 里的丢包：

例如：
>increase backlog to 3000 with sysctl.
```bash
$ sudo sysctl -w net.core.netdev_max_backlog=3000
```
默认值是 1000。

>Tuning: Adjust the NAPI weight of the backlog poll loop

`net.core.dev_weight` 决定了 backlog poll loop 可以消耗的整体 budget
```bash
$ sudo sysctl -w net.core.dev_weight=600
```
默认值是 64。

backlog 处理逻辑和设备驱动的 poll 函数类似，都是在软中断（softirq）的上下文中执行，因此受整体 budget 和处理时间的限制。

>Tuning: Enabling flow limits and tuning flow limit hash table size
```bash
$ sudo sysctl -w net.core.flow_limit_table_len=8192
```
默认值是 4096.

这只会影响新分配的 flow hash table。所以，如果你想增加 table size 的话，应该在打开 flow limit 功能之前设置这个值。

打开 flow limit 功能的方式是，在`/proc/sys/net/core/flow_limit_cpu_bitmap` 中指定一 个 bitmask，和通过 bitmask 打开 RPS 的操作类似。

#### 4.2.3 处理 backlog 队列：NAPI poller
每个 CPU 都有一个 backlog queue，其加入到 NAPI 变量的方式和驱动差不多，都是注册一个 poll 方法，在软中断的上下文中处理包。此外，还提供了一个 weight，这也和驱动类似 。注册发生在网络系统初始化的时候，
net/core/dev.c的 net_dev_init 函数：
```c
sd->backlog.poll = process_backlog;
sd->backlog.weight = weight_p;
sd->backlog.gro_list = NULL;
sd->backlog.gro_count = 0;
```
backlog NAPI 变量和设备驱动 NAPI 变量的不同之处在于，它的 weight 是可以调节的，而设备驱动是 hardcode 64。
#### 4.2.4 `process_backlog`
`process_backlog` 是一个循环，它会一直运行直至 weight用完，或者 backlog 里没有数据了。

backlog queue 里的数据取出来，传递给`__netif_receive_skb`。这个函数做的事情和 RPS 关闭的情况下做的事情一样。即，`__netif_receive_skb` 做一些 bookkeeping 工作，然后调用`__netif_receive_skb_core` 将数据发送给更上面的协议层。

`process_backlog` 和 NAPI 之间遵循的合约，和驱动和 NAPI 之间的合约相同：
>NAPI is disabled if the total weight will not be used. The poller is restarted with the call to ____napi_schedule from enqueue_to_backlog as described above.

函数返回接收完成的数据帧数量（在代码中是变量 work），`net_rx_action`将会从 budget（通过 `net.core.netdev_budget` 可以调整）里减去这个值。


#### 4.2.5 `__netif_receive_skb_core`：将数据送到抓包点（tap）或协议层
`__netif_receive_skb_core` 完成将数据送到协议栈这一繁重工作（the heavy lifting of delivering the data)。在此之前，它会先检查是否插入了 packet tap（探测点），这些 tap 是抓包用的。例如，`AF_PACKET` 地址族就可以插入这些抓包指令， 一般通过 libpcap 库。

如果存在抓包点（tap），数据就会先到抓包点，然后才到协议层。


#### 4.2.6 送到抓包点（tap）
如果有 packet tap（通常通过 libpcap），packet 会送到那里。 net/core/dev.c:
```c
list_for_each_entry_rcu(ptype, &ptype_all, list) {
	if (!ptype->dev || ptype->dev == skb->dev) {
    	if (pt_prev)
      		ret = deliver_skb(skb, pt_prev, orig_dev);
   		pt_prev = ptype;
	}
}
```
packet 如何经过 pcap 可以阅读 net/packet/af_packet.c。


#### 4.2.7 送到协议层
处理完 taps 之后，`__netif_receive_skb_core` 将数据发送到协议层。它会从数据包中取出协议信息，然后遍历注册在这个协议上的回调函数列表。可以看`__netif_receive_skb_core` 函数，net/core/dev.c:
```c
type = skb->protocol;
list_for_each_entry_rcu(ptype,
                &ptype_base[ntohs(type) & PTYPE_HASH_MASK], list) {
        if (ptype->type == type &&
            (ptype->dev == null_or_dev || ptype->dev == skb->dev ||
             ptype->dev == orig_dev)) {
                if (pt_prev)
                        ret = deliver_skb(skb, pt_prev, orig_dev);
                pt_prev = ptype;
        }
}
```
上面的 `ptype_base` 是一个 hash table，定义在net/core/dev.c中:
```
struct list_head ptype_base[PTYPE_HASH_SIZE] __read_mostly;
```
每种协议在上面的 hash table 的一个 slot 里，添加一个过滤器到列表里。这个列表的头用如下函数获取：
```c
static inline struct list_head *ptype_head(const struct packet_type *pt)
{
        if (pt->type == htons(ETH_P_ALL))
                return &ptype_all;
        else
                return &ptype_base[ntohs(pt->type) & PTYPE_HASH_MASK];
}
```
添加的时候用 `dev_add_pack` 这个函数。这就是协议层如何注册自身，用于处理相应协议的网络数据的。

### 4.3 RPS 调优
使用 RPS 需要在内核做配置（Ubuntu + Kernel 3.13.0 支持），而且需要一个掩码（ bitmask）指定哪些 CPU 可以处理那些 RX 队列。相关的一些信息可以在内核文档里找到。

bitmask 配置位于：`/sys/class/net/DEVICE_NAME/queues/QUEUE/rps_cpus`

例如，对于 eth0 的 queue 0，你需要更改`/sys/class/net/eth0/queues/rx-0/rps_cpus`。 内核文档里说，对一些特定的配置下，RPS 没必要了。

注意：打开 RPS 之后，原来不需要处理软中断（softirq）的 CPU 这时也会参与处理。因此相应 CPU 的 `NET_RX` 数量，以及 si 或 sitime 占比都会相应增加。可以对比启用 RPS 前后的数据，以此来确定配置是否生效，以及是否符合预期（哪个 CPU 处理哪个网卡的哪个中断）。

## 5. 总结
本文大篇幅在分析RPS的工作原理。RPS 的工作原理是对个 packet 做 hash，以此决定分到哪个 CPU 处理。然后 packet 放到每个 CPU 独占的接收后备队列（backlog）等待处理。这个 CPU 会触发一个进程间中断（ IPI，Inter-processor Interrupt）向对端 CPU。如果当时对端 CPU 没有在处理 backlog 队列收包，这个进程间中断会 触发它开始从 backlog 收包。`/proc/net/softnet_stat` 其中有一列是记录 `softnet_data` 变量（也即这个 CPU）收到了多少 IPI（received_rps 列）。因此，`netif_receive_skb` 或者继续将包送到协议栈，或者交给 RPS，后者会转交给其他 CPU 处理。

参考资料：

https://blog.packagecloud.io/eng/2016/06/22/monitoring-tuning-linux-networking-stack-receiving-data/

https://blog.csdn.net/cloudvtech/article/details/80182074

https://www.coder.work/article/448092



