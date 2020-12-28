---
title: "在生产环境中使用 eBPF 调试 Go 程序"
date: 2020-12-27T23:44:59+08:00
author: "Zain Asgar, 陈恒奇译"
keywords: ["BPF", "eBPF", "Go", "Golang"]
categories : ["BPF"]
banner : "img/blogimg/default.png"
summary : "本文翻译自 https://blog.pixielabs.ai/ebpf-function-tracing/post/"
---

# 第 1 部分: 在生产环境中使用 eBPF 调试 Go 程序

这是本系列文章的第一篇，讲述了我们如何在生产环境中使用 eBPF 调试应用程序而无需重新编译/重新部署。这篇文章介绍了如何使用 [gobpf](https://github.com/iovisor/gobpf) 和 uprobe 来为 Go 程序构建函数参数跟踪程序。这项技术也可以扩展应用于其他编译型语言，例如 C++，Rust 等。本系列的后续文章将讨论如何使用 eBPF 来跟踪 HTTP/gRPC/SSL 等。

## 简介

在调试时，我们通常对了解程序的状态感兴趣。这使我们能够检查程序正在做什么，并确定缺陷在代码中的位置。观察状态的一种简单方法是使用调试器来捕获函数的参数。对于 Go 程序来说，我们经常使用 Delve 或者 GDB。

在开发环境中，Delve 和 GDB 工作得很好，但是在生产环境中并不经常使用它们。那些使调试器强大的特性也让它们不适合在生产环境中使用。调试器会导致程序中断，甚至允许修改状态，这可能会导致软件产生意外故障。

为了更好地捕获函数参数，我们将探索使用 [eBPF](https://ebpf.io/)（在 Linux 4.x+ 中可用）以及高级的 Go 程序库 [gobpf](https://github.com/iovisor/gobpf)。

## eBPF 是什么？

扩展的 BPF(eBPF) 是 Linux 4.x+ 里的一项内核技术。你可以把它想像成一个运行在 Linux 内核中的轻量级的沙箱虚拟机，可以提供对内核内存的经过验证的访问。

如下概述所示，eBPF 允许内核运行 BPF 字节码。尽管使用的前端语言可能会有所不同，但它通常是 C 的受限子集。一般情况下，使用 Clang 将 C 代码编译为 BPF 字节码，然后验证这些字节码，确保可以安全运行。这些严格的验证确保了机器码不会有意或无意地破坏 Linux 内核，并且 BPF 探针每次被触发时，都只会执行有限的指令。这些保证使 eBPF 可以用于性能关键的工作负载，例如数据包过滤，网络监控等。

从功能上讲，eBPF 允许你在某些事件（例如定时器，网络事件或函数调用）触发时运行受限的 C 代码。当在函数调用上触发时，我们称这些函数为探针，它们既可以用于内核里的函数调用(kprobe) 也可以用于用户态程序中的函数调用(uprobe)。本文重点介绍使用 uprobe 来动态跟踪函数参数。

## Uprobe

uprobe 可以通过插入触发软中断的调试陷阱指令（x86 上的 int3）来拦截用户态程序。这也是[调试器的工作方式](https://eli.thegreenplace.net/2011/01/27/how-debuggers-work-part-2-breakpoints)。uprobe 的流程与任何其他 BPF 程序基本相同，如下图所示。经过编译和验证的 BPF 程序将作为 uprobe 的一部分执行，并且可以将结果写入缓冲区。

![BPF for tracing (from Brendan Gregg)](https://blog.pixielabs.ai/static/a11d6d9cb78e055d59136a97665907d3/073a0/bpf-tracing.jpg)

让我们看看 uprobe 是如何工作的。要部署 uprobe 并捕获函数参数，我们将使用[这个](https://github.com/pixie-labs/pixie/blob/main/demos/simple-gotracing/app/app.go)简单的示例程序。这个 Go 程序的相关部分如下所示。

main() 是一个简单的 HTTP 服务器，在路径 /e 上公开单个 GET 端点，该端点使用迭代逼近来计算欧拉数(e)。computeE接受单个查询参数(iterations)，该参数指定计算近似值要运行的迭代次数。迭代次数越多，近似值越准确，但会消耗指令周期。理解函数背后的数学并不是必需的。我们只是想跟踪对 computeE 的任何调用的参数。

```golang
// computeE computes the approximation of e by running a fixed number of iterations.
func computeE(iterations int64) float64 {
  res := 2.0
  fact := 1.0

  for i := int64(2); i < iterations; i++ {
    fact *= float64(i)
    res += 1 / fact
  }
  return res
}

func main() {
  http.HandleFunc("/e", func(w http.ResponseWriter, r *http.Request) {
    // Parse iters argument from get request, use default if not available.
    // ... removed for brevity ...
    w.Write([]byte(fmt.Sprintf("e = %0.4f\n", computeE(iters))))
  })
  // Start server...
}
```

要了解 uprobe 的工作原理，让我们看一下二进制文件中如何跟踪符号。由于 uprobe 通过插入调试陷阱指令来工作，因此我们需要获取函数所在的地址。Linux 上的 Go 二进制文件使用 ELF 存储调试信息。除非删除了调试数据，否则即使在优化过的二进制文件中也可以找到这些信息。我们可以使用 objdump 命令检查二进制文件中的符号：

```bash
[0] % objdump --syms app|grep computeE
00000000006609a0 g     F .text    000000000000004b              main.computeE
```

从这个输出中，我们知道函数 computeE 位于地址 0x6609a0。要看到它前后的指令，我们可以使用 objdump 来反汇编二进制文件（通过添加 -d 选项实现）。反汇编后的代码如下：

```bash
[0] % objdump -d app | less
00000000006609a0 <main.computeE>:
  6609a0:       48 8b 44 24 08          mov    0x8(%rsp),%rax
  6609a5:       b9 02 00 00 00          mov    $0x2,%ecx
  6609aa:       f2 0f 10 05 16 a6 0f    movsd  0xfa616(%rip),%xmm0
  6609b1:       00
  6609b2:       f2 0f 10 0d 36 a6 0f    movsd  0xfa636(%rip),%xmm1
```

由此可见，当 computeE 被调用时会发生什么。第一条指令是 mov 0x8(%rsp), %rax。它把 rsp 寄存器偏移 0x8 的内容移动到 rax 寄存器。这实际上就是上面的输入参数 iterations。 Go 的参数在栈上传递。

有了这些信息，我们现在就可以继续深入，编写代码来跟踪 computeE 的参数了。

## 构建跟踪程序

要捕获事件，我们需要注册一个 uprobe 函数，还需要一个可以读取输出的用户空间函数。如下图所示。我们将编写一个称为跟踪程序的二进制文件，它负责注册 BPF 代码并读取 BPF 代码的结果。如图所示，uprobe 简单地写入 perf buffer，这是用于 perf 事件的 Linux 内核数据结构。

![High-level overview showing the Tracer binary listening to perf events generated from the App](https://blog.pixielabs.ai/static/9f8b26f88f9b132440ef1b9d48b5a341/app-tracer.svg)

现在，我们已了解了涉及到的各个部分，下面让我们详细研究添加 uprobe 时发生的情况。下图显示了 Linux 内核如何使用uprobe 修改二进制文件。软中断指令(int3)作为第一条指令被插入 main.computeE 中。这将导致软中断，从而允许 Linux 内核执行我们的 BPF 函数。然后我们将参数写入 perf buffer，该缓冲区由跟踪程序异步读取。

![Details of how a debug trap instruction is used call a BPF program](https://blog.pixielabs.ai/static/87301c7282e8f8270fee2afb9fe85c81/app-trace.svg)

BPF 函数相对简单，C代码如下所示。我们注册这个函数，每次调用 main.computeE 时都将调用它。一旦调用，我们只需读取函数参数并写入 perf buffer。设置缓冲区需要很多样板代码，可以在[完整的示例](https://github.com/pixie-labs/pixie/blob/main/demos/simple-gotracing/trace_example/trace.go)中找到。

```c
#include <uapi/linux/ptrace.h>

BPF_PERF_OUTPUT(trace);

inline int computeECalled(struct pt_regs *ctx) {
  // The input argument is stored in ax.
  long val = ctx->ax;
  trace.perf_submit(ctx, &val, sizeof(val));
  return 0;
}
```

现在我们有了一个用于 main.computeE 函数的功能完善的端到端的参数跟踪程序！下面的视频片段展示了这一结果。

![End-to-End demo](https://blog.pixielabs.ai/static/4de8713a5b05e1f9132350f333572174/e2e-demo.gif)

另一个很棒的事情是，我们可以使用 GDB 来查看对二进制文件所做的修改。在运行我们的跟踪程序之前，我们输出地址 0x6609a0 的指令。

```bash
(gdb) display /4i 0x6609a0
10: x/4i 0x6609a0
   0x6609a0 <main.computeE>:    mov    0x8(%rsp),%rax
   0x6609a5 <main.computeE+5>:  mov    $0x2,%ecx
   0x6609aa <main.computeE+10>: movsd  0xfa616(%rip),%xmm0
   0x6609b2 <main.computeE+18>: movsd  0xfa636(%rip),%xmm1
```

而这是在我们运行跟踪程序之后。我们可以清楚地看到，第一个指令现在变成 int3 了。

```bash
(gdb) display /4i 0x6609a0
7: x/4i 0x6609a0
   0x6609a0 <main.computeE>:    int3
   0x6609a1 <main.computeE+1>:  mov    0x8(%rsp),%eax
   0x6609a5 <main.computeE+5>:  mov    $0x2,%ecx
   0x6609aa <main.computeE+10>: movsd  0xfa616(%rip),%xmm0
```

尽管我们为该特定示例对跟踪程序进行了硬编码，但是这个过程是可以通用化的。Go 的许多方面（例如嵌套指针，接口，通道等）让这个过程变得有挑战性，但是解决这些问题可以使用现有系统中不存在的另一种检测模式。另外，因为这一过程工作在二进制层面，它也可以用于其他语言（C++，Rust 等）编译的二进制文件。我们只需考虑它们各自 ABI 的差异。

## 下一步是什么？

使用 uprobe 进行 BPF 跟踪有其自身的优缺点。当我们需要观察二进制程序的状态时，BPF 很有用，甚至在连接调试器会产生问题或者坏处的环境（例如生产环境二进制程序）。 最大的缺点是，即使是最简单的程序状态的观测性，也需要编写代码来实现。编写和维护 BPF 代码很复杂。没有大量高级工具，不太可能把它当作一般的调试手段。


* 原文链接: https://blog.pixielabs.ai/ebpf-function-tracing/post/
* 翻译已获作者同意，版权归原作者所有
