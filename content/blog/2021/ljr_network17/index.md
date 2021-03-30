---
title: "Linux内核网络性能优化"
date: 2021-03-30T14:30:45+08:00
author: "梁金荣"
keywords: ["内核网络"]
categories : ["内核网络"]
banner : "img/blogimg/ljrimg6.jpg"
summary : "本文只总结了常见的网络优化技术，并使用尽量多的图片帮助理解原理，但未深入具体去介绍原理和具体用法，目的是以后需要时可以宏观上知道该怎么去做，进而微观上深入内核来解决问题。"
---

## 1. 前言
本文将简单介绍Linux内核网络协议栈的流程，并总结常见的网络优化技术，使用尽量多的图片帮助理解原理，感谢阅读。
## 2. Linux网络协议栈
数据包在内核中使用`sk_buff`结构体来传递。网络套接字是用`sock`结构体来定义的，该结构体在各网络协议结构体的开头部分存放，例如`tcp_sock`。网络协议使用`proto`结构体挂载到网络套接字结构体上，例如`tcp_prot`、`udp_prot`等，该结构体中定义了一系列该网络协议需要的回调函数，包括`connect`、`sendmsg`、`recvmsg`等。

通常Linux内核网络协议栈接收和发送数据时的流程是酱样子的：

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210326161338757.png)

以接收数据包为例，可以概况为：

- 加载网卡驱动，初始化
- 数据包从外部网络进入网卡
- 网卡（通过DMA）将包拷贝到内核内存中的ring buffer
- 产生硬件中断，通知系统收到了一个包
- 驱动调用 NAPI ，如果轮询（poll）还没有开始，就开始轮询
- ksoftirqd软中断调用 NAPI 的poll函数从ring buffer收包（poll 函数是网卡驱动在初始化阶段注册的；每个cpu上都运行着一个ksoftirqd进程，在系统启动期间就注册了）
- ring buffer里面对应的内存区域解除映射（unmapped）
- 如果 packet steering 功能打开，或者网卡有多队列，网卡收到的数据包会被分发到多个cpu
- 数据包从队列进入协议层
- 协议层处理数据包
- 数据包从协议层进入相应 socket 的接收队列
- 应用程序从socket拿到数据包

## 3. DPDK

为了提高数据包处理能力，进而提高网络性能，出现了DPDK这样的内核协议栈绕过绕过技术。DPDK需要应用程序在用户态实现自己的网络协议栈，这样可以直接向网卡设备驱动程序发送数据，也可以直接从网卡内存中读取数据包，通过避免数据的多次复制来提高网络性能。

**缺点**：因为DPDK绕过了内核中的整个网络协议栈，所以无法使用传统工具进行跟踪和性能分析。
<img src="https://img-blog.csdnimg.cn/20210326162331423.png" alt="在这里插入图片描述" style="zoom:50%;" />

## 4. XDP
XDP（eXpress Data Path）为Linux内核提供了高性能、可编程的网络数据路径。XDP使用网卡驱动程序中内置的BPF钩子直接访问原始网络帧数据，直接告诉网卡应该传递还是丢弃数据包，避免了TCP/IP协议栈处理的额外消耗。因为网络包在未进入网络协议栈之前就处理，所以它给Linux网络带来了巨大的性能提升（性能比DPDK还要高）。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20210329103033114.png)

### 4.1 XDP主要的特性

- 在网络协议栈前处理
- 无锁设计
- 批量I/O操作
- 轮询式
- 直接队列访问
- 不需要分配skbuff
- 支持网络卸载
- DDIO
- XDP程序快速执行并结束，没有循环
- Packeting steering

### 4.2 XDP与DPDK的对比

优点：

- 无需第三方代码库和许可
- 同时支持轮询式和中断式网络
- 无需分配大页
- 无需专用的CPU
- 无需定义新的安全网络模型
- 可以回退到正常网络栈处理过程
- 
缺点：

- XDP不提供缓存队列（qdisc），TX设备太慢时直接丢包
- XDP程序是专用的，不具备网络协议栈的通用性

### 4.3 应用场景
- 快速DDoS缓解
- 软件定义路由（SDR）

## 5. CPU负载均衡
通常状况下，CPU处理网络数据包，一个单独的网卡一般只会向一个CPU发送中断，这可能导致该CPU资源全部用于处理中断和网络协议栈，进而成为全系统的性能瓶颈。下面将介绍一些常见的用于处理网络数据包的CPU负载均衡技术：
### 5.1 NAPI
在NAPI架构中，当接收到数据包产生中断时，驱动程序会通知网络子系统有新的数据包到来（而不是立即处理数据包），这样就可以在ISR（Interrupt Service Routines - 中断服务程序）上下文之外使用轮询的方式来一次性接收多个数据包。
![在这里插入图片描述](https://img-blog.csdnimg.cn/20210330171048898.png)
驱动程序不再使用数据包接收队列，网卡本身需要维护一个缓冲区来保存接收到数据包，并且可以禁止中断。这种方法减少了中断的产生并且在突发情况下减少了丢包的可能性，避免了接收队列的饱和，进而提升了网络性能。

### 5.2 RSS(receive side scaling):网卡多队列
RSS(Receive Side Scaling)是一种能够在多处理器系统下使接收报文在多个CPU之间高效分发的网卡驱动技术。

- 网卡对接收到的报文进行解析，获取IP地址、协议和端口五元组信息；
- 网卡通过配置的HASH函数根据五元组信息计算出HASH值，也可以根据二、三或四元组进行计算；
- 取HASH值的低几位(不同网卡可能不同)作为RETA(redirection table)的索引；
- 根据RETA中存储的值分发到对应的CPU。

RSS需要硬件支持。基于RSS技术程序可以通过硬件在多个CPU之间来分发数据流，并且可以通过对RETA的修改来实现动态的负载均衡。

### 5.3 RPS(receive packet Steering):RSS的软件实现
RPS(Receive Package Steering)其原理是以软件方式实现接收的报文在cpu之间平均分配，即利用报文的hash值找到匹配的cpu，然后将报文送至该cpu对应的backlog队列中进行下一步的处理。

报文hash值，可以是由网卡计算得到，也可以是由软件计算得到，具体的计算也因报文协议不同而有所差异，以tcp报文为例，tcp报文的hash值是根据四元组信息，即源IP地址、源端口、目的IP地址和目的端口进行hash计算得到的。

RPS是接收报文的时候处理，而XPS是发送报文的时候处理器优化。

### 5.4 XPS(transmit packet Steering):应用在发送方向
较上述RPS，XPS是软件支持的发数据包时的多队列，
### 5.5 RFS(receive flwo Steering): 基于flow的RPS
RPS只是根据报文的hash值从分发处理报文的cpu列表中选取一个目标cpu，这样虽然负载均衡的效果很好，但是当用户态处理报文的cpu和内核处理报文软中断的cpu不同的时候，就会导致cpu的缓存不命中，影响性能。而RFS(Receive Flow Steering)就是用来处理这种情况的，RFS的目标是通过指派处理报文的应用程序所在的cpu来在内核态处理报文，以此来增加cpu的缓存命中率。
## 6. 总结
本文只总结了常见的网络优化技术，并使用尽量多的图片帮助理解原理，但未深入具体去介绍原理和用法，目的是以后需要时可以宏观上知道该怎么去做，进而微观上深入内核来解决问题。

参考链接：

https://www.cnblogs.com/sammyliu/p/5225623.html

https://blog.selectel.com/introduction-dpdk-architecture-principles/

https://medium.com/@jain.sm/express-data-path-xdp-introduction-d41b77ffbabf

https://tonydeng.github.io/sdn-handbook/linux/XDP/

http://cxd2014.github.io/2017/10/15/linux-napi/

https://chengqian90.com/%E6%9D%82%E8%B0%88/%E7%BD%91%E7%BB%9CRPS-RFS-GSO-GRO%E7%AD%89%E5%8A%9F%E8%83%BD%E9%87%8A%E4%B9%89.html

