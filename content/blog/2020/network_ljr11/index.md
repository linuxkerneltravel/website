---
title: "Linux内核网络数据包发送（一）"
date: 2020-09-28T14:07:28+08:00
author: "梁金荣"
keywords: ["内核网络"]
categories : ["内核网络"]
banner : "img/blogimg/ljrimg2.jpg"
summary : "本文首先从宏观上概述了数据包发送的流程，接着分析了协议层注册进内核以及被socket的过程，最后介绍了通过 socket 发送网络数据的过程。"
---

# 1. 前言

本文首先从宏观上概述了数据包发送的流程，接着分析了协议层注册进内核以及被socket的过程，最后介绍了通过 socket 发送网络数据的过程。

# 2. 数据包发送宏观视角

从宏观上看，一个数据包从用户程序到达硬件网卡的整个过程如下：

1. 使用**系统调用**（如 `sendto`，`sendmsg` 等）写数据
2. 数据穿过**socket 子系统**，进入**socket 协议族**（protocol family）系统
3. 协议族处理：数据穿过**协议层**，这一过程（在许多情况下）会将**数据**（data）转换成**数据包**（packet）
4. 数据穿过**路由层**，这会涉及路由缓存和 ARP 缓存的更新；如果目的 MAC 不在 ARP 缓存表中，将触发一次 ARP 广播来查找 MAC 地址
5. 穿过协议层，packet 到达**设备无关层**（device agnostic layer）
6. 使用 XPS（如果启用）或散列函数**选择发送队列**
7. 调用网卡驱动的**发送函数**
8. 数据传送到网卡的 `qdisc`（queue discipline，排队规则）
9. qdisc 会直接**发送数据**（如果可以），或者将其放到队列，下次触发**NET_TX 类型软中断**（softirq）的时候再发送
10. 数据从 qdisc 传送给驱动程序
11. 驱动程序创建所需的**DMA 映射**，以便网卡从 RAM 读取数据
12. 驱动向网卡发送信号，通知**数据可以发送了**
13. **网卡从 RAM 中获取数据并发送**
14. 发送完成后，设备触发一个**硬中断**（IRQ），表示发送完成
15. **硬中断处理函数**被唤醒执行。对许多设备来说，这会**触发 NET_RX 类型的软中断**，然后 NAPI poll 循环开始收包
16. poll 函数会调用驱动程序的相应函数，**解除 DMA 映射**，释放数据

# 3. 协议层注册

协议层分析我们将关注 IP 和 UDP 层，其他协议层可参考这个过程。我们首先来看协议族是如何注册到内核，并被 socket 子系统使用的。

当用户程序像下面这样创建 UDP socket 时会发生什么？

```c
sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP)
```

简单来说，内核会去查找由 UDP 协议栈导出的一组函数（其中包括用于发送和接收网络数据的函数），并赋给 socket 的相应字段。准确理解这个过程需要查看 `AF_INET` 地址族的代码。

内核初始化的很早阶段就执行了 `inet_init` 函数，这个函数会注册 `AF_INET` 协议族 ，以及该协议族内的各协议栈（TCP，UDP，ICMP 和 RAW），并调用初始化函数使协议栈准备好处理网络数据。`inet_init` 定义在net/ipv4/af_inet.c 。

`AF_INET` 协议族导出一个包含 `create` 方法的 `struct net_proto_family` 类型实例。当从用户程序创建 socket 时，内核会调用此方法：

```c
static const struct net_proto_family inet_family_ops = {
    .family = PF_INET,
    .create = inet_create,
    .owner  = THIS_MODULE,
};
```

`inet_create` 根据传递的 socket 参数，在已注册的协议中查找对应的协议：

```c
/* Look for the requested type/protocol pair. */
lookup_protocol:
        err = -ESOCKTNOSUPPORT;
        rcu_read_lock();
        list_for_each_entry_rcu(answer, &inetsw[sock->type], list) {

                err = 0;
                /* Check the non-wild match. */
                if (protocol == answer->protocol) {
                        if (protocol != IPPROTO_IP)
                                break;
                } else {
                        /* Check for the two wild cases. */
                        if (IPPROTO_IP == protocol) {
                                protocol = answer->protocol;
                                break;
                        }
                        if (IPPROTO_IP == answer->protocol)
                                break;
                }
                err = -EPROTONOSUPPORT;
        }
```

然后，将该协议的回调方法（集合）赋给这个新创建的 socket：

```c
sock->ops = answer->ops;
```

可以在 `af_inet.c` 中看到所有协议的初始化参数。 下面是TCP 和 UDP的初始化参数：

```c
/* Upon startup we insert all the elements in inetsw_array[] into
 * the linked list inetsw.
 */
static struct inet_protosw inetsw_array[] =
{
        {
                .type =       SOCK_STREAM,
                .protocol =   IPPROTO_TCP,
                .prot =       &tcp_prot,
                .ops =        &inet_stream_ops,
                .no_check =   0,
                .flags =      INET_PROTOSW_PERMANENT |
                              INET_PROTOSW_ICSK,
        },

        {
                .type =       SOCK_DGRAM,
                .protocol =   IPPROTO_UDP,
                .prot =       &udp_prot,
                .ops =        &inet_dgram_ops,
                .no_check =   UDP_CSUM_DEFAULT,
                .flags =      INET_PROTOSW_PERMANENT,
       },

            /* .... more protocols ... */
```

`IPPROTO_UDP` 协议类型有一个 `ops` 变量，包含很多信息，包括用于发送和接收数据的回调函数：

```c
const struct proto_ops inet_dgram_ops = {
	.family          = PF_INET,
	.owner           = THIS_MODULE,
	
	/* ... */
	
	.sendmsg     = inet_sendmsg,
	.recvmsg     = inet_recvmsg,
	
	/* ... */
};
EXPORT_SYMBOL(inet_dgram_ops);
```

`prot` 字段指向一个协议相关的变量（的地址），对于 UDP 协议，其中包含了 UDP 相关的回调函数。 UDP 协议对应的 `prot` 变量为 `udp_prot`，定义在 net/ipv4/udp.c：

```c
struct proto udp_prot = {
	.name        = "UDP",
	.owner           = THIS_MODULE,
	
	/* ... */
	
	.sendmsg     = udp_sendmsg,
	.recvmsg     = udp_recvmsg,
	
	/* ... */
};
EXPORT_SYMBOL(udp_prot);
```

现在，让我们转向发送 UDP 数据的用户程序，看看 `udp_sendmsg` 是如何在内核中被调用的。

# 4. 通过 socket 发送网络数据

用户程序想发送 UDP 网络数据，因此它使用 `sendto` 系统调用：

```c
ret = sendto(socket, buffer, buflen, 0, &dest, sizeof(dest));
```

该系统调用穿过Linux 系统调用（system call）层，最后到达net/socket.c中的这个函数：

```c
/*
 *      Send a datagram to a given address. We move the address into kernel
 *      space and check the user space data area is readable before invoking
 *      the protocol.
 */

SYSCALL_DEFINE6(sendto, int, fd, void __user *, buff, size_t, len,
                unsigned int, flags, struct sockaddr __user *, addr,
                int, addr_len)
{
    /*  ... code ... */

    err = sock_sendmsg(sock, &msg, len);

    /* ... code  ... */
}
```

`SYSCALL_DEFINE6` 宏会展开成一堆宏，后者经过一波复杂操作创建出一个带 6 个参数的系统调用（因此叫 `DEFINE6`）。作为结果之一，会看到内核中的所有系统调用都带 `sys_`前缀。

`sendto` 代码会先将数据整理成底层可以处理的格式，然后调用 `sock_sendmsg`。特别地， 它将传递给 `sendto` 的地址放到另一个变量（`msg`）中：

```c
iov.iov_base = buff;
iov.iov_len = len;
msg.msg_name = NULL;
msg.msg_iov = &iov;
msg.msg_iovlen = 1;
msg.msg_control = NULL;
msg.msg_controllen = 0;
msg.msg_namelen = 0;
if (addr) {
        err = move_addr_to_kernel(addr, addr_len, &address);
        if (err < 0)
                goto out_put;
        msg.msg_name = (struct sockaddr *)&address;
        msg.msg_namelen = addr_len;
}
```

这段代码将用户程序传入到内核的（存放待发送数据的）地址，作为 `msg_name` 字段嵌入到 `struct msghdr` 类型变量中。这和用户程序直接调用 `sendmsg` 而不是 `sendto` 发送数据差不多，这之所以可行，是因为 `sendto` 和 `sendmsg` 底层都会调用 `sock_sendmsg`。

## 4.1 `sock_sendmsg`, `__sock_sendmsg`, `__sock_sendmsg_nosec`

`sock_sendmsg` 做一些错误检查，然后调用`__sock_sendmsg`；后者做一些自己的错误检查 ，然后调用`__sock_sendmsg_nosec`。`__sock_sendmsg_nosec` 将数据传递到 socket 子系统的更深处：

```c
static inline int __sock_sendmsg_nosec(struct kiocb *iocb, struct socket *sock,
                                       struct msghdr *msg, size_t size)
{
    struct sock_iocb *si =  ....

    /* other code ... */

    return sock->ops->sendmsg(iocb, sock, msg, size);
}
```

通过前面介绍的 socket 创建过程，可以知道注册到这里的 `sendmsg` 方法就是 `inet_sendmsg`。



## 4.2 `inet_sendmsg`

从名字可以猜到，这是 `AF_INET` 协议族提供的通用函数。 此函数首先调用 `sock_rps_record_flow` 来记录最后一个处理该（数据所属的）flow 的 CPU; Receive Packet Steering 会用到这个信息。接下来，调用 socket 的协议类型（本例是 UDP）对应的 `sendmsg` 方法：

```c
int inet_sendmsg(struct kiocb *iocb, struct socket *sock, struct msghdr *msg,
                 size_t size)
{
      struct sock *sk = sock->sk;

      sock_rps_record_flow(sk);

      /* We may need to bind the socket. */
      if (!inet_sk(sk)->inet_num && !sk->sk_prot->no_autobind && inet_autobind(sk))
              return -EAGAIN;

      return sk->sk_prot->sendmsg(iocb, sk, msg, size);
}
EXPORT_SYMBOL(inet_sendmsg);
```

本例是 UDP 协议，因此上面的 `sk->sk_prot->sendmsg` 指向的是之前看到的（通过 `udp_prot` 导出的）`udp_sendmsg` 函数。

**sendmsg()函数作为分界点，处理逻辑从 AF_INET 协议族通用处理转移到具体的 UDP 协议的处理。**

# 5. 总结

了解Linux内核网络数据包发送的详细过程，有助于我们进行网络监控和调优。本文只分析了协议层的注册和通过 socket 发送数据的过程，数据在传输层和网络层的详细发送过程将在下一篇文章中分析。

参考链接：

[1] https://blog.packagecloud.io/eng/2017/02/06/monitoring-tuning-linux-networking-stack-sending-data

[2] https://segmentfault.com/a/1190000008926093
