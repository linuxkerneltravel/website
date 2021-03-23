---
title: "Linux内核网络数据发送（六）——网络设备驱动"
date: 2020-11-23T10:07:28+08:00
author: "梁金荣"
keywords: ["内核网络"]
categories : ["内核网络"]
banner : "img/blogimg/ljrimg17.jpg"
summary : "本文主要介绍设备通过 DMA 从 RAM 中读取数据并将其发送到网络，主要分析`dev_hard_start_xmit` 通过调用 `ndo_start_xmit`来发送数据的过程。"
---

# 1. 前言

本文主要介绍设备通过 DMA 从 RAM 中读取数据并将其发送到网络，主要分析`dev_hard_start_xmit` 通过调用 `ndo_start_xmit`来发送数据的过程。

# 2. 驱动回调函数注册

驱动程序实现了一系列方法来支持设备操作，例如：

1. 发送数据（`ndo_start_xmit`）
2. 获取统计信息（`ndo_get_stats64`）
3. 处理设备 `ioctl`s（`ndo_do_ioctl`）

这些方法通过一个 `struct net_device_ops` 实例导出。看igb 驱动程序中这些操作：

```c
static const struct net_device_ops igb_netdev_ops = {
        .ndo_open               = igb_open,
        .ndo_stop               = igb_close,
        .ndo_start_xmit         = igb_xmit_frame,
        .ndo_get_stats64        = igb_get_stats64,

                /* ... more fields ... */
};
```

这个 `igb_netdev_ops` 变量在 `igb_probe`函数中注册给设备：

```c
static int igb_probe(struct pci_dev *pdev, const struct pci_device_id *ent)
{
                /* ... lots of other stuff ... */

        netdev->netdev_ops = &igb_netdev_ops;

                /* ... more code ... */
}
```



# 3.  `ndo_start_xmit` 发送数据

上层的网络栈通过 `struct net_device_ops` 实例里的回调函数，调用驱动程序来执行各种操作。正如我们之前看到的，qdisc 代码调用 `ndo_start_xmit` 将数据传递给驱动程序进行发送。对于大多数硬件设备，都是在保持一个锁时调用 `ndo_start_xmit` 函数。

在 igb 设备驱动程序中，`ndo_start_xmit` 字段初始化为 `igb_xmit_frame` 函数，所以接下来从 `igb_xmit_frame` 开始，查看该驱动程序是如何发送数据的。在 drivers/net/ethernet/intel/igb/igb_main.c中，以下代码在整个执行过程中都 hold 着一个锁：

```c
netdev_tx_t igb_xmit_frame_ring(struct sk_buff *skb,
                                struct igb_ring *tx_ring)
{
        struct igb_tx_buffer *first;
        int tso;
        u32 tx_flags = 0;
        u16 count = TXD_USE_COUNT(skb_headlen(skb));
        __be16 protocol = vlan_get_protocol(skb);
        u8 hdr_len = 0;

        /* need: 1 descriptor per page * PAGE_SIZE/IGB_MAX_DATA_PER_TXD,
         *       + 1 desc for skb_headlen/IGB_MAX_DATA_PER_TXD,
         *       + 2 desc gap to keep tail from touching head,
         *       + 1 desc for context descriptor,
         * otherwise try next time
         */
        if (NETDEV_FRAG_PAGE_MAX_SIZE > IGB_MAX_DATA_PER_TXD) {
                unsigned short f;
                for (f = 0; f < skb_shinfo(skb)->nr_frags; f++)
                        count += TXD_USE_COUNT(skb_shinfo(skb)->frags[f].size);
        } else {
                count += skb_shinfo(skb)->nr_frags;
        }
```

函数首先使用 `TXD_USER_COUNT` 宏来计算发送 skb 所需的描述符数量，用 `count` 变量表示。然后根据分片情况，对 `count` 进行相应调整。

```c
        if (igb_maybe_stop_tx(tx_ring, count + 3)) {
                /* this is a hard error */
                return NETDEV_TX_BUSY;
        }
```

然后驱动程序调用内部函数 `igb_maybe_stop_tx`，检查 TX Queue 以确保有足够可用的描述符。如果没有，则返回 `NETDEV_TX_BUSY`。这将导致 qdisc 将 skb 重新入队以便稍后重试。

```c
        /* record the location of the first descriptor for this packet */
        first = &tx_ring->tx_buffer_info[tx_ring->next_to_use];
        first->skb = skb;
        first->bytecount = skb->len;
        first->gso_segs = 1;
```

然后，获取 TX Queue 中下一个可用缓冲区信息，用 `struct igb_tx_buffer *first` 表 示，这个信息稍后将用于设置缓冲区描述符。数据包 `skb` 指针及其大小 `skb->len` 也存储到 `first`。

```c
        skb_tx_timestamp(skb);
```

接下来代码调用 `skb_tx_timestamp`，获取基于软件的发送时间戳。应用程序可以 使用发送时间戳来确定数据包通过网络栈的发送路径所花费的时间。某些设备还支持硬件时间戳，这允许系统将打时间戳任务 offload 到设备。程序员因此可以 获得更准确的时间戳，因为它更接近于硬件实际发送的时间。

某些网络设备可以使用Precision Time Protocol（PTP，精确时间协议）在硬件中为数据包加时间戳。驱动程序处理用户的硬件时间戳请求。现在看这个代码：

```c
        if (unlikely(skb_shinfo(skb)->tx_flags & SKBTX_HW_TSTAMP)) {
                struct igb_adapter *adapter = netdev_priv(tx_ring->netdev);

                if (!(adapter->ptp_tx_skb)) {
                        skb_shinfo(skb)->tx_flags |= SKBTX_IN_PROGRESS;
                        tx_flags |= IGB_TX_FLAGS_TSTAMP;

                        adapter->ptp_tx_skb = skb_get(skb);
                        adapter->ptp_tx_start = jiffies;
                        if (adapter->hw.mac.type == e1000_82576)
                                schedule_work(&adapter->ptp_tx_work);
                }
        }
```

上面的 if 语句检查 `SKBTX_HW_TSTAMP` 标志，该标志表示用户请求了硬件时间戳。接下来检 查是否设置了 `ptp_tx_skb`。一次只能给一个数据包加时间戳，因此给正在打时间戳的 skb 上设置了 `SKBTX_IN_PROGRESS` 标志。然后更新 `tx_flags`，将 `IGB_TX_FLAGS_TSTAMP` 标志 置位。`tx_flags` 变量稍后将被复制到缓冲区信息结构中。

当前的 `jiffies` 值赋给 `ptp_tx_start`。驱动程序中的其他代码将使用这个值， 以确保 TX 硬件打时间戳不会 hang 住。最后，如果这是一个 82576 以太网硬件网卡，将用 `schedule_work` 函数启动工作队列。

```c
        if (vlan_tx_tag_present(skb)) {
                tx_flags |= IGB_TX_FLAGS_VLAN;
                tx_flags |= (vlan_tx_tag_get(skb) << IGB_TX_FLAGS_VLAN_SHIFT);
        }
```

上面的代码将检查 skb 的 `vlan_tci` 字段是否设置了，如果是，将设置 `IGB_TX_FLAGS_VLAN` 标记，并保存 VLAN ID。

```c
        /* record initial flags and protocol */
        first->tx_flags = tx_flags;
        first->protocol = protocol;
```

最后将 `tx_flags` 和 `protocol` 值都保存到 `first` 变量里面。

```c
        tso = igb_tso(tx_ring, first, &hdr_len);
        if (tso < 0)
                goto out_drop;
        else if (!tso)
                igb_tx_csum(tx_ring, first);
```

接下来，驱动程序调用其内部函数 `igb_tso`，判断 skb 是否需要分片。如果需要 ，缓冲区信息变量（`first`）将更新标志位，以提示硬件需要做 TSO。

如果不需要 TSO，则 `igb_tso` 返回 0；否则返回 1。 如果返回 0，则将调用 `igb_tx_csum` 来 处理校验和 offload 信息（是否需要 offload，是否支持此协议的 offload）。 `igb_tx_csum` 函数将检查 skb 的属性，修改 `first` 变量中的一些标志位，以表示需要校验和 offload。

```c
        igb_tx_map(tx_ring, first, hdr_len);
```

`igb_tx_map` 函数准备给设备发送的数据。我们后面会仔细查看这个函数。

```c
        /* Make sure there is space in the ring for the next send. */
        igb_maybe_stop_tx(tx_ring, DESC_NEEDED);

        return NETDEV_TX_OK;
```

发送结束之后，驱动要检查确保有足够的描述符用于下一次发送。如果不够，TX Queue 将被 关闭。最后返回 `NETDEV_TX_OK` 给上层（qdisc 代码）。

```c
out_drop:
        igb_unmap_and_free_tx_resource(tx_ring, first);

        return NETDEV_TX_OK;
}
```

最后是一些错误处理代码，只有当 `igb_tso` 遇到某种错误时才会触发此代码。 `igb_unmap_and_free_tx_resource` 用于清理数据。在这种情况下也返回 `NETDEV_TX_OK` 。发送没有成功，但驱动程序释放了相关资源，没有什么需要做的了。在这种情况下，此驱动程序不会增加 drop 计数，但或许它应该增加。



# 4.  `igb_tx_map`

`igb_tx_map` 函数处理将 skb 数据映射到 RAM 的 DMA 区域的细节。它还会更新设备 TX Queue 的 尾部指针，从而触发设备“被唤醒”，从 RAM 获取数据并开始发送。看一下这个函数的工作原理：

```c
static void igb_tx_map(struct igb_ring *tx_ring,
                       struct igb_tx_buffer *first,
                       const u8 hdr_len)
{
        struct sk_buff *skb = first->skb;

                /* ... other variables ... */

        u32 tx_flags = first->tx_flags;
        u32 cmd_type = igb_tx_cmd_type(skb, tx_flags);
        u16 i = tx_ring->next_to_use;

        tx_desc = IGB_TX_DESC(tx_ring, i);

        igb_tx_olinfo_status(tx_ring, tx_desc, tx_flags, skb->len - hdr_len);

        size = skb_headlen(skb);
        data_len = skb->data_len;

        dma = dma_map_single(tx_ring->dev, skb->data, size, DMA_TO_DEVICE);
```

上面的代码所做的一些事情：

1. 声明变量并初始化
2. 使用 `IGB_TX_DESC` 获取下一个可用描述符的指针
3. `igb_tx_olinfo_status` 函数更新 `tx_flags`，并将它们复制到描述符（`tx_desc`）中
4. 计算 skb 头长度和数据长度
5. 调用 `dma_map_single` 为 `skb->data` 构造内存映射，以允许设备通过 DMA 从 RAM 中读取数据

接下来是驱动程序中的一个**非常长的循环，用于为 skb 的每个分片生成有效映射**。具体如何做的细节并不是特别重要，但如下步骤值得一提：

- 驱动程序遍历该数据包的所有分片
- 当前描述符有其数据的 DMA 地址信息
- 如果分片的大小大于单个 IGB 描述符可以发送的大小，则构造多个描述符指向可 DMA 区域的块，直到描述符指向整个分片
- 更新描述符迭代器
- 更新剩余长度
- 当没有剩余分片或者已经消耗了整个数据长度时，循环终止

下面提供循环的代码以供以上描述参考。这里的代码进一步向读者说明，**如果可能的话，避免分片是一个好主意**。分片需要大量额外的代码来处理网络栈的每一层，包括驱动层。

```c
        tx_buffer = first;

        for (frag = &skb_shinfo(skb)->frags[0];; frag++) {
                if (dma_mapping_error(tx_ring->dev, dma))
                        goto dma_error;

                /* record length, and DMA address */
                dma_unmap_len_set(tx_buffer, len, size);
                dma_unmap_addr_set(tx_buffer, dma, dma);

                tx_desc->read.buffer_addr = cpu_to_le64(dma);

                while (unlikely(size > IGB_MAX_DATA_PER_TXD)) {
                        tx_desc->read.cmd_type_len =
                                cpu_to_le32(cmd_type ^ IGB_MAX_DATA_PER_TXD);

                        i++;
                        tx_desc++;
                        if (i == tx_ring->count) {
                                tx_desc = IGB_TX_DESC(tx_ring, 0);
                                i = 0;
                        }
                        tx_desc->read.olinfo_status = 0;

                        dma += IGB_MAX_DATA_PER_TXD;
                        size -= IGB_MAX_DATA_PER_TXD;

                        tx_desc->read.buffer_addr = cpu_to_le64(dma);
                }

                if (likely(!data_len))
                        break;

                tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type ^ size);

                i++;
                tx_desc++;
                if (i == tx_ring->count) {
                        tx_desc = IGB_TX_DESC(tx_ring, 0);
                        i = 0;
                }
                tx_desc->read.olinfo_status = 0;

                size = skb_frag_size(frag);
                data_len -= size;

                dma = skb_frag_dma_map(tx_ring->dev, frag, 0,
                                       size, DMA_TO_DEVICE);

                tx_buffer = &tx_ring->tx_buffer_info[i];
        }
```

所有需要的描述符都已建好，且 `skb` 的所有数据都映射到 DMA 地址后，驱动就会进入到它的最后一步，触发一次发送：

```c
        /* write last descriptor with RS and EOP bits */
        cmd_type |= size | IGB_TXD_DCMD;
        tx_desc->read.cmd_type_len = cpu_to_le32(cmd_type);
```

对最后一个描述符设置 `RS` 和 `EOP` 位，以提示设备这是最后一个描述符了。

```c
        netdev_tx_sent_queue(txring_txq(tx_ring), first->bytecount);

        /* set the timestamp */
        first->time_stamp = jiffies;
```

调用 `netdev_tx_sent_queue` 函数，同时带着将发送的字节数作为参数。这个函数是 byte query limit（字节查询限制）功能的一部分，当前的 jiffies 存储到 `first` 的时间戳字段。

```c
        /* Force memory writes to complete before letting h/w know there
         * are new descriptors to fetch.  (Only applicable for weak-ordered
         * memory model archs, such as IA-64).
         *
         * We also need this memory barrier to make certain all of the
         * status bits have been updated before next_to_watch is written.
         */
        wmb();

        /* set next_to_watch value indicating a packet is present */
        first->next_to_watch = tx_desc;

        i++;
        if (i == tx_ring->count)
                i = 0;

        tx_ring->next_to_use = i;

        writel(i, tx_ring->tail);

        /* we need this if more than one processor can write to our tail
         * at a time, it synchronizes IO on IA64/Altix systems
         */
        mmiowb();

        return;
```

上面的代码做了一些重要的事情：

1. 调用 `wmb` 函数强制完成内存写入。这通常称作**“写屏障”**（write barrier） ，是通过 CPU 平台相关的特殊指令完成的。这对某些 CPU 架构非常重要，因为如果触发 设备启动 DMA 时不能确保所有内存写入已经完成，那设备可能从 RAM 中读取不一致 状态的数据。
2. 设置 `next_to_watch` 字段，它将在 completion 阶段后期使用
3. 更新计数，并且 TX Queue 的 `next_to_use` 字段设置为下一个可用的描述符。使用 `writel` 函数更新 TX Queue 的尾部。`writel` 向内存映射 I/O地址写入一个 `long` 型数据 ，这里地址是 `tx_ring->tail`（一个硬件地址），要写入的值是 `i`。这次写操作会让 设备知道其他数据已经准备好，可以通过 DMA 从 RAM 中读取并写入网络
4. 最后，调用 `mmiowb` 函数。它执行特定于 CPU 体系结构的指令，对内存映射的 写操作进行排序。它也是一个写屏障，用于内存映射的 I/O 写

最后，代码包含了一些错误处理。只有 DMA API（将 skb 数据地址映射到 DMA 地址）返回错误时，才会执行此代码。

```c
dma_error:
        dev_err(tx_ring->dev, "TX DMA map failed\n");

        /* clear dma mappings for failed tx_buffer_info map */
        for (;;) {
                tx_buffer = &tx_ring->tx_buffer_info[i];
                igb_unmap_and_free_tx_resource(tx_ring, tx_buffer);
                if (tx_buffer == first)
                        break;
                if (i == 0)
                        i = tx_ring->count;
                i--;
        }

        tx_ring->next_to_use = i;
```
参考资料：https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data
