---
title: "kprobes/kretprobes 在bcc程序中的使用"
date: 2020-06-21T14:30:45+08:00
author: "Jinrong"
keywords: ["eBPF"]
categories : ["eBPF"]
banner : "img/blogimg/ljrimg10.jpg"
summary : "本文简单介绍了kprobes和kretprobes机制，以获取系统中的 TCP IPv4 连接信息的bcc代码为例，介绍了kprobes/kretprobes 在 bcc 程序中的使用，并给出了运行结果和分析。"
---

本文简单介绍了kprobes和kretprobes机制，以获取系统中的 TCP IPv4 连接信息的bcc代码为例，介绍了kprobes/kretprobes 在 bcc 程序中的使用，并给出了运行结果和分析。

# 1. kprobes/kretprobes 介绍
## 1.1 kprobes 介绍
kprobes 主要用来对内核进行调试追踪， 属于比较轻量级的机制,，本质上是在指定的探测点（比如函数的某行， 函数的入口地址和出口地址,，或者内核的指定地址处）插入一组处理程序.。内核执行到这组处理程序的时候就可以获取到当前正在执行的上下文信息， 比如当前的函数名， 函数处理的参数以及函数的返回值,，也可以获取到寄存器甚至全局数据结构的信息。
## 1.2 kretprobes 介绍
kretprobes 在 kprobes 的机制上实现，主要用于返回点（比如内核函数或者系统调用的返回值）的探测以及函数执行耗时的计算。

# 2. bcc实例代码 
下面举例来介绍 kprobe/kretprobes 在 BPF C 程序中的使用，本实例的功能为获取系统中的 TCP IPv4 连接信息。

```c
from __future__ import print_function
from bcc import BPF

# define BPF program
bpf_text = """
#include <uapi/linux/ptrace.h>
#include <net/sock.h>
#include <bcc/proto.h>
BPF_HASH(currsock, u32, struct sock *);
int kprobe__tcp_v4_connect(struct pt_regs *ctx, struct sock *sk)
{
	u32 pid = bpf_get_current_pid_tgid();
	// stash the sock ptr for lookup on return
	currsock.update(&pid, &sk);
	return 0;
};
int kretprobe__tcp_v4_connect(struct pt_regs *ctx)
{
	int ret = PT_REGS_RC(ctx);
	u32 pid = bpf_get_current_pid_tgid();
	struct sock **skpp;
	skpp = currsock.lookup(&pid);
	if (skpp == 0) {
		return 0;	// missed entry
	}
	if (ret != 0) {
		// failed to send SYNC packet, may not have populated
		// socket __sk_common.{skc_rcv_saddr, ...}
		currsock.delete(&pid);
		return 0;
	}
	// pull in details
	struct sock *skp = *skpp;
	u32 saddr = 0, daddr = 0;
	u16 dport = 0;
	bpf_probe_read(&saddr, sizeof(saddr), &skp->__sk_common.skc_rcv_saddr);
	bpf_probe_read(&daddr, sizeof(daddr), &skp->__sk_common.skc_daddr);
	bpf_probe_read(&dport, sizeof(dport), &skp->__sk_common.skc_dport);
	// output
	bpf_trace_printk("trace_tcp4connect %x %x %d\\n", saddr, daddr, ntohs(dport));
	currsock.delete(&pid);
	return 0;
}
"""

# initialize BPF
b = BPF(text=bpf_text)

# header
print("%-6s %-12s %-16s %-16s %-4s" % ("PID", "COMM", "SADDR", "DADDR",
    "DPORT"))

def inet_ntoa(addr):
	dq = ''
	for i in range(0, 4):
		dq = dq + str(addr & 0xff)
		if (i != 3):
			dq = dq + '.'
		addr = addr >> 8
	return dq

# filter and format output
while 1:
        # Read messages from kernel pipe
        try:
            (task, pid, cpu, flags, ts, msg) = b.trace_fields()
            (_tag, saddr_hs, daddr_hs, dport_s) = msg.split(" ")
        except ValueError:
            # Ignore messages from other tracers
            continue

        # Ignore messages from other tracers
        if _tag != "trace_tcp4connect":
            continue

	print("%-6d %-12.12s %-16s %-16s %-4s" % (pid, task,
	    inet_ntoa(int(saddr_hs, 16)),
	    inet_ntoa(int(daddr_hs, 16)),
	    dport_s))
```

# 3. 实例代码分析
## 3.1 kprobes
语法格式：``kprobe__kernel_function_name``

其中`kprobe__`是前缀，用于给内核函数创建一个kprobe(内核函数调用的动态跟踪)。也可通过C语言函数定义一个C函数，然后使用 python 的`BPF.attach_kprobe()`来关联到内核函数。

本实例中使用 kprobes 定义了这样一个函数：
```c
int  kprobe__tcp_v4_connect（struct pt_regs * ctx，struct sock * sk）
{
    [...]
}
```
参数如下：
- `struct pt_regs *ctx`：寄存器和BPF文件；
- `struct sock *sk`：`tcp_v4_connect` 内核函数的第一个参数。

`tcp_v4_connect`在内核中的定义如下：

```c
int tcp_v4_connect(struct sock *sk, struct sockaddr *uaddr, int addr_len);
```

第一个参数总是 `struct pt_regs *`，其余的参数就是这个内核函数的参数（如果不使用的话，就不用写了）。

## 3.2 kretprobes
kretprobes 用于动态跟踪内核函数的返回，语法如下：
语法格式：``kretprobe__kernel_function_name``

其中`kretprobe__`是前缀，用来创建 kretprobe 对内核函数返回的动态追踪。也可通过C语言函数定义一个C函数，然后使用 python 的`BPF.attach_kprobe()`来关联到内核函数。

本实例中使用 kretprobes 定义了这样一个函数：
```c
int kretprobe__tcp_v4_connect(struct pt_regs *ctx)
{
    int ret = PT_REGS_RC(ctx);
    [...]
}
```
返回的参数可以在 BPF C 程序中使用`PT_REGS_RC()`来获得，返回值保存在了ret中。
# 4. 运行结果
使用如下命令运行实例bcc程序，运行结果如图：
```bash
sudo python tcp4connect.py
```
![在这里插入图片描述](https://img-blog.csdnimg.cn/20200621152849336.png)
可以看到 bcc 程序成功获取到了系统 TCP IPv4连接信息，运行结果中标题解释如下：
|列标题|含义|
|-|-|
|PID|进程号|
|COMM|进程命令行|
|SADDR|源地址|
|DADDR|目标地址|
|DPORT|目标端口|

参考链接：
https://lwn.net/Articles/132196/
https://github.com/iovisor/bcc/blob/master/docs/reference_guide.md
