---
title: "Linux 线程相关（1）－Linux 线程模型的比较：LinuxThreads 和 NPTL"
date: 2008-11-10T16:50:24+08:00
author: "helight"
keywords: ["线程","NPTL"]
categories : ["新手上路"]
banner : "img/blogimg/3.png"
summary : "LinuxThreads 项目最初将多线程的概念引入了 Linux®，但是 LinuxThreads 并不遵守 POSIX 线程标准。尽管更新的 Native POSIX Thread Library（NPTL）库填补了一些空白，但是这仍然存在一些问题。本文为那些需要将自己的应用程序从 LinuxThreads 移植到 NPTL 上或者只是希望理解有何区别的开发人员介绍这两种 Linux 线程模型之间的区别。"
---

​		当 Linux 最初开发时，在内核中并不能真正支持线程。但是它的确可以通过 `clone()` 系统调用将进程作为可调度的实体。这个调用创建了调用进程（calling process）的一个拷贝，这个拷贝与调用进程共享相同的地址空间。LinuxThreads 项目使用这个调用来完全在用户空间模拟对线程的支持。不幸的是，这种方法有一些缺点，尤其是在信号处理、调度和进程间同步原语方面都存在问题。另外，这个 线程模型也不符合 POSIX 的要求。 要改进 LinuxThreads，非常明显我们需要内核的支持，并且需要重写线程库。有两个相互竞争的项目开始来满足这些要求。一个包括 IBM 的开发人员的团队开展了 NGPT（Next-Generation POSIX Threads）项目。同时，Red Hat 的一些开发人员开展了 NPTL 项目。NGPT 在 2003 年中期被放弃了，把这个领域完全留给了 NPTL。 尽管从 LinuxThreads 到 NPTL 看起来似乎是一个必然的过程，但是如果您正在为一个历史悠久的 Linux 发行版维护一些应用程序，并且计划很快就要进行升级，那么如何迁移到 NPTL 上就会变成整个移植过程中重要的一个部分。另外，我们可能会希望了解二者之间的区别，这样就可以对自己的应用程序进行设计，使其能够更好地利用这两种技 术。 本文详细介绍了这些线程模型分别是在哪些发行版上实现的。 *线程* 将应用程序划分成一个或多个同时运行的任务。线程与传统的多任务*进程* 之间的区别在于：线程共享的是单个进程的状态信息，并会直接共享内存和其他资源。同一个进程中线程之间的上下文切换通常要比进程之间的上下文切换速度更 快。因此，多线程程序的优点就是它可以比多进程应用程序的执行速度更快。另外，使用线程我们可以实现并行处理。这些相对于基于进程的方法所具有的优点推动 了 LinuxThreads 的实现。 LinuxThreads 最初的设计相信相关进程之间的上下文切换速度很快，因此每个内核线程足以处理很多相关的用户级线程。这就导致了*一对一* 线程模型的革命。 让我们来回顾一下 LinuxThreads 设计细节的一些基本理念：

- LinuxThreads 非常出名的一个特性就是管理线程（manager thread）管理线程可以满足以下要求：
  - 系统必须能够响应终止信号并杀死整个进程。
  - 以堆栈形式使用的内存回收必须在线程完成之后进行。因此，线程无法自行完成这个过程。
  - 终止线程必须进行等待，这样它们才不会进入僵尸状态。
  - 线程本地数据的回收需要对所有线程进行遍历；这必须由管理线程来进行。
  - 如果主线程需要调用 `pthread_exit()`，那么这个线程就无法结束。主线程要进入睡眠状态，而管理线程的工作就是在所有线程都被杀死之后来唤醒这个主线程。
- 为了维护线程本地数据和内存，LinuxThreads 使用了进程地址空间的高位内存（就在堆栈地址之下）。
- 原语的同步是使用*信号* 来实现的。例如，线程会一直阻塞，直到被信号唤醒为止。
- 在克隆系统的最初设计之下，LinuxThreads 将每个线程都是作为一个具有惟一进程 ID 的进程实现的。
- 终止信号可以杀死所有的线程。LinuxThreads 接收到终止信号之后，管理线程就会使用相同的信号杀死所有其他线程（进程）。
- 根据 LinuxThreads 的设计，如果一个异步信号被发送了，那么管理线程就会将这个信号发送给一个线程。如果这个线程现在阻塞了这个信号，那么这个信号也就会被挂起。这是因为管理线程无法将这个信号发送给进程；相反，每个线程都是作为一个进程在执行。

LinuxThreads 的设计通常都可以很好地工作；但是在压力很大的应用程序中，它的性能、可伸缩性和可用性都会存在问题。下面让我们来看一下 LinuxThreads 设计的一些局限性：

- 它使用管理线程来创建线程，并对每个进程所拥有的所有线程进行协调。这增加了创建和销毁线程所需要的开销。
- 由于它是围绕一个管理线程来设计的，因此会导致很多的上下文切换的开销，这可能会妨碍系统的可伸缩性和性能。
- 由于管理线程只能在一个 CPU 上运行，因此所执行的同步操作在 SMP 或 NUMA 系统上可能会产生可伸缩性的问题。
- 由于线程的管理方式，以及每个线程都使用了一个不同的进程 ID，因此 LinuxThreads 与其他与 POSIX 相关的线程库并不兼容。
- 信号用来实现同步原语，这会影响操作的响应时间。另外，将信号发送到主进程的概念也并不存在。因此，这并不遵守 POSIX 中处理信号的方法。
- LinuxThreads 中对信号的处理是按照每线程的原则建立的，而不是按照每进程的原则建立的，这是因为每个线程都有一个独立的进程 ID。由于信号被发送给了一个专用的线程，因此信号是*串行化的* —— 也就是说，信号是透过这个线程再传递给其他线程的。这与 POSIX 标准对线程进行并行处理的要求形成了鲜明的对比。例如，在 LinuxThreads 中，通过 `kill()` 所发送的信号被传递到一些单独的线程，而不是集中整体进行处理。这意味着如果有线程阻塞了这个信号，那么 LinuxThreads 就只能对这个线程进行排队，并在线程开放这个信号时在执行处理，而不是像其他没有阻塞信号的线程中一样立即处理这个信号。
- 由于 LinuxThreads 中的每个线程都是一个进程，因此用户和组 ID 的信息可能对单个进程中的所有线程来说都不是通用的。例如，一个多线程的 `setuid()`/`setgid()` 进程对于不同的线程来说可能都是不同的。
- 有一些情况下，所创建的多线程核心转储中并没有包含所有的线程信息。同样，这种行为也是每个线程都是一个进程这个事实所导致的结果。如果任何线程 发生了问题，我们在系统的核心文件中只能看到这个线程的信息。不过，这种行为主要适用于早期版本的 LinuxThreads 实现。
- 由于每个线程都是一个单独的进程，因此 /proc 目录中会充满众多的进程项，而这实际上应该是线程。
- 由于每个线程都是一个进程，因此对每个应用程序只能创建有限数目的线程。例如，在 IA32 系统上，可用进程总数 —— 也就是可以创建的线程总数 —— 是 4,090。
- 由于计算线程本地数据的方法是基于堆栈地址的位置的，因此对于这些数据的访问速度都很慢。另外一个缺点是用户无法可信地指定堆栈的大小，因为用户可能会意外地将堆栈地址映射到本来要为其他目的所使用的区域上了。按需增长（grow on demand）的概念（也称为浮动堆栈的概念）是在 2.4.10 版本的 Linux 内核中实现的。在此之前，LinuxThreads 使用的是固定堆栈。

NPTL，或称为 Native POSIX Thread Library，是 Linux 线程的一个新实现，它克服了 LinuxThreads 的缺点，同时也符合 POSIX 的需求。与 LinuxThreads 相比，它在性能和稳定性方面都提供了重大的改进。与 LinuxThreads 一样，NPTL 也实现了一对一的模型。 Ulrich Drepper 和 Ingo Molnar 是 Red Hat 参与 NPTL 设计的两名员工。他们的总体设计目标如下：

- 这个新线程库应该兼容 POSIX 标准。
- 这个线程实现应该在具有很多处理器的系统上也能很好地工作。
- 为一小段任务创建新线程应该具有很低的启动成本。
- NPTL 线程库应该与 LinuxThreads 是二进制兼容的。注意，为此我们可以使用 `LD_ASSUME_KERNEL`，这会在本文稍后进行讨论。
- 这个新线程库应该可以利用 NUMA 支持的优点。与 LinuxThreads 相比，NPTL 具有很多优点：
- NPTL 没有使用管理线程。管理线程的一些需求，例如向作为进程一部分的所有线程发送终止信号，是并不需要的；因为内核本身就可以实现这些功能。内核还会处理每个 线程堆栈所使用的内存的回收工作。它甚至还通过在清除父线程之前进行等待，从而实现对所有线程结束的管理，这样可以避免僵尸进程的问题。
- 由于 NPTL 没有使用管理线程，因此其线程模型在 NUMA 和 SMP 系统上具有更好的可伸缩性和同步机制。
- 使用 NPTL 线程库与新内核实现，就可以避免使用信号来对线程进行同步了。为了这个目的，NPTL 引入了一种名为 *futex* 的新机制。futex 在共享内存区域上进行工作，因此可以在进程之间进行共享，这样就可以提供进程间 POSIX 同步机制。我们也可以在进程之间共享一个 futex。这种行为使得进程间同步成为可能。实际上，NPTL 包含了一个 `PTHREAD_PROCESS_SHARED` 宏，使得开发人员可以让用户级进程在不同进程的线程之间共享互斥锁。
- 由于 NPTL 是 POSIX 兼容的，因此它对信号的处理是按照每进程的原则进行的；`getpid()` 会为所有的线程返回相同的进程 ID。例如，如果发送了 `SIGSTOP` 信号，那么整个进程都会停止；使用 LinuxThreads，只有接收到这个信号的线程才会停止。这样可以在基于 NPTL 的应用程序上更好地利用调试器，例如 GDB。
- 由于在 NPTL 中所有线程都具有一个父进程，因此对父进程汇报的资源使用情况（例如 CPU 和内存百分比）都是对整个进程进行统计的，而不是对一个线程进行统计的。

正如上面介绍的一样，ABI 的引入使得可以同时支持 NPTL 和 LinuxThreads 模型。基本上来说，这是通过 ld （一个动态链接器/加载器）来进行处理的，它会决定动态链接到哪个运行时线程库上。 举例来说，下面是 WebSphere® Application Server 对这个变量所使用的一些通用设置；您可以根据自己的需要进行适当的设置：

- `LD_ASSUME_KERNEL=2.4.19`：这会覆盖 NPTL 的实现。这种实现通常都表示使用标准的 LinuxThreads 模型，并启用浮动堆栈的特性。
- `LD_ASSUME_KERNEL=2.2.5`：这会覆盖 NPTL 的实现。这种实现通常都表示使用 LinuxThreads 模型，同时使用固定堆栈大小。

我们可以使用下面的命令来设置这个变量： `export LD_ASSUME_KERNEL=2.4.19` 注意，对于任何 `LD_ASSUME_KERNEL` 设置的支持都取决于目前所支持的线程库的 ABI 版本。例如，如果线程库并不支持 2.2.5 版本的 ABI，那么用户就不能将 `LD_ASSUME_KERNEL` 设置为 2.2.5。通常，NPTL 需要 2.4.20，而 LinuxThreads 则需要 2.4.1。 如果您正运行的是一个启用了 NPTL 的 Linux 发行版，但是应用程序却是基于 LinuxThreads 模型来设计的，那么所有这些设置通常都可以使用。

大部分现代 Linux 发行版都预装了 LinuxThreads 和 NPTL，因此它们提供了一种机制来在二者之间进行切换。要查看您的系统上正在使用的是哪个线程库，请运行下面的命令： `$ getconf GNU_LIBPTHREAD_VERSION` 这会产生类似于下面的输出结果： `NPTL 0.34` 或者： `linuxthreads-0.10`

表 1 列出了一些流行的 Linux 发行版，以及它们所采用的线程实现的类型、glibc 库和内核版本。

|               线程实现               |     C 库     |              发行版              |  内核  |
| :----------------------------------: | :----------: | :------------------------------: | :----: |
|  LinuxThreads 0.7, 0.71 (for libc5)  |   libc 5.x   |           Red Hat 4.2            |        |
| LinuxThreads 0.7, 0.71 (for glibc 2) | glibc 2.0.x  |           Red Hat 5.x            |        |
|           LinuxThreads 0.8           | glibc 2.1.1  |           Red Hat 6.0            |        |
|           LinuxThreads 0.8           | glibc 2.1.2  |       Red Hat 6.1 and 6.2        |        |
|           LinuxThreads 0.9           |              |           Red Hat 7.2            | 2.4.7  |
|           LinuxThreads 0.9           | glibc 2.2.4  |          Red Hat 2.1 AS          | 2.4.9  |
|          LinuxThreads 0.10           | glibc 2.2.93 |           Red Hat 8.0            | 2.4.18 |
|               NPTL 0.6               |  glibc 2.3   |           Red Hat 9.0            | 2.4.20 |
|              NPTL 0.61               | glibc 2.3.2  |          Red Hat 3.0 EL          | 2.4.21 |
|              NPTL 2.3.4              | glibc 2.3.4  |           Red Hat 4.0            | 2.6.9  |
|           LinuxThreads 0.9           |  glibc 2.2   | SUSE Linux Enterprise Server 7.1 | 2.4.18 |
|           LinuxThreads 0.9           | glibc 2.2.5  |  SUSE Linux Enterprise Server 8  | 2.4.21 |
|           LinuxThreads 0.9           | glibc 2.2.5  |           United Linux           | 2.4.21 |
|              NPTL 2.3.5              | glibc 2.3.3  |  SUSE Linux Enterprise Server 9  | 2.6.5  |

注意，从 2.6.x 版本的内核和 glibc 2.3.3 开始，NPTL 所采用的版本号命名约定发生了变化：这个库现在是根据所使用的 glibc 的版本进行编号的。 Java™ 虚拟机（JVM）的支持可能会稍有不同。IBM 的 JVM 可以支持表 1 中 glibc 版本高于 2.1 的大部分发行版。

LinuxThreads 的限制已经在 NPTL 以及 LinuxThreads 后期的一些版本中得到了克服。例如，最新的 LinuxThreads 实现使用了线程注册来定位线程本地数据；例如在 Intel® 处理器上，它就使用了 `%fs` 和 `%gs` 段寄存器来定位访问线程本地数据所使用的虚拟地址。尽管这个结果展示了 LinuxThreads 所采纳的一些修改的改进结果，但是它在更高负载和压力测试中，依然存在很多问题，因为它过分地依赖于一个管理线程，使用它来进行信号处理等操作。 您应该记住，在使用 LinuxThreads 构建库时，需要使用 `-D_REENTRANT` 编译时标志。这使得库线程是安全的。 最后，也许是最重要的事情，请记住 LinuxThreads 项目的创建者已经不再积极更新它了，他们认为 NPTL 会取代 LinuxThreads。 LinuxThreads 的缺点并不意味着 NPTL 就没有错误。作为一个面向 SMP 的设计，NPTL 也有一些缺点。我曾经看到过在最近的 Red Hat 内核上出现过这样的问题：一个简单线程在单处理器的机器上运行良好，但在 SMP 机器上却挂起了。我相信在 Linux 上还有更多工作要做才能使它具有更好的可伸缩性，从而满足高端应用程序的需求。

- 您可以参阅本文在 developerWorks 全球站点上的 [英文原文](http://www.ibm.com/developerworks/linux/library/l-threading.html?S_TACT=105AGX52&S_CMP=cn-a-l) 。
- Ulrich Drepper 和 Ingo Molnar 编写的 “[The Native POSIX Thread Library for Linux](http://people.redhat.com/drepper/nptl-design.pdf)”（PDF）介绍了设计 NPTL 的原因和目标，其中包括了 LinuxThreads 的缺点和 NPTL 的优点。
- [LinuxThreads FAQ](http://pauillac.inria.fr/~xleroy/linuxthreads/faq.html) 包含了有关 LinuxThreads 和 NPTL 的常见问题。这对于了解早期的 LinuxThreads 实现的缺点来说是一个很好的资源。
- Ulrich Drepper 撰写的 “[Explaining LD_ASSUME_KERNEL](http://people.redhat.com/drepper/assumekernel.html)” 提供了有关这个环境变量的详细介绍。
- “[Native POSIX Threading Library (NPTL) support](http://publib.boulder.ibm.com/infocenter/wasinfo/v5r1//index.jsp?topic=/com.ibm.websphere.base.doc/info/aes/ae/cins_nptl.html)” 从 WebSphere 的视角介绍了 LinuxThreads 和 NPTL 之间的区别，并解释了 WebSphere Application Server 如何支持这两种不同的线程模型。
- [Diagnosis documentation for IBM ports of the JVM](http://www.ibm.com/developerworks/java/jdk/diagnosis/?S_TACT=105AGX52&S_CMP=cn-a-l) 定义了 Java 应用程序在 Linux 上运行时面临问题时所要搜集的诊断信息。
- 在 [developerWorks Linux 专区](http://www.ibm.com/developerworks/cn/linux/) 中可以找到为 Linux 开发人员准备的更多资源。
- 随时关注 [developerWorks 技术事件和网络广播](http://www.ibm.com/developerworks/offers/techbriefings/?S_TACT=105AGX52&S_CMP=cn-a-l)。