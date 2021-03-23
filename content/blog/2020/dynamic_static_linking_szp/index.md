---
title: "动态链接与静态链接"
date: 2020-11-02T15:54:52+08:00
author: "孙张品"
keywords: ["动态链接","静态链接"]
categories : ["内存管理"]
banner : "img/blogimg/static_linking.jpg"
summary : "本文对内存管理一章学习内容进行补充和记录，包括进程地址空间的分配、与撤销（mmap，munmap），动态链接与静态链接的区别，静态链接简单实验。"
---
# 1. 概述

本文对内存管理一章学习内容进行补充和记录，包括进程地址空间的分配、与撤销（mmap，munmap），动态链接与静态链接的区别，静态链接简单实验。

# 2. 虚拟内存、内核空间和用户空间

32位平台上，线性空间的大小为4GB，Linux将4G的空间分为两部分。最高位的1GB（从虚地址0xC0000000到0xFFFFFFFF）供内核使用，称为“内核空间”。而较低的3GB（从虚地址0x00000000到0xBFFFFFFF），供进程使用，称为“用户空间”。因为内核空间由系统内的所有进程共享，所以每个进程可以拥有4GB的虚拟地址空间，其中0GB-3GB是进程私有空间，这个空间对其他进程不可见，最高的1GB内核空间为所有进程以及内核共享。

# 3. 进程的地址空间

进程执行指令需要代码、数据、堆栈。
+ 代码（main,%rip会从此处取出待执行的指令）
+ 数据（static int x）
+ 堆栈（int x）
  + 可以用指针访问
+ 动态链接库
+ 运行时分配的内存

进程地址空间是一段一段连续的内存，每一段都有自己的职责，拥有相应的访问权限。

Linux提供mmap系统调用，可以为进程虚拟地址空间创建一个新的段，这个段可以是硬盘中某个文件的映射，也可以是匿名的数据，用来分配内存。munmap用于移除地址空间中的某一个段，mprotect用于修改某个段的权限。

```c
#include <sys/mman.h>

void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
/*start：映射区的开始地址，设置为0时表示由系统决定映射区的起始地址。
 length：映射区的长度。
 prot：期望的内存保护标志，不能与文件的打开模式冲突。是以下的某个值，可以通过or运算（“|”）合理地组合在一起
  PROT_EXEC //页内容可以被执行
  PROT_READ //页内容可以被读取
  PROT_WRITE //页可以被写入
  PROT_NONE //页不可访问
 flags：指定映射对象的类型，映射选项和映射页是否可以共享。它的值可以是一个或者多个以下位的组合体
  MAP_FIXED //使用指定的映射起始地址，如果由start和len参数指定的内存区重叠于现存的映射空间，重叠部分将会被丢弃。如果指定的起始地址不可用，操作将会失败。
     //并且起始地址必须落在页的边界上。
  MAP_SHARED //与其它所有映射这个对象的进程共享映射空间。对共享区的写入，相当于输出到文件。直到msync()或者munmap()被调用，文件实际上不会被更新。
  MAP_PRIVATE //建立一个写入时拷贝的私有映射。内存区域的写入不会影响到原文件。这个标志和以上标志是互斥的，只能使用其中一个。
  MAP_DENYWRITE //这个标志被忽略。
  MAP_EXECUTABLE //同上
  MAP_NORESERVE //不要为这个映射保留交换空间。当交换空间被保留，对映射区修改的可能会得到保证。当交换空间不被保留，同时内存不足，对映射区的修改会引起段违例信号。
  MAP_LOCKED //锁定映射区的页面，从而防止页面被交换出内存。
  MAP_GROWSDOWN //用于堆栈，告诉内核VM系统，映射区可以向下扩展。
  MAP_ANONYMOUS //匿名映射，映射区不与任何文件关联。
  MAP_ANON //MAP_ANONYMOUS的别称，不再被使用。
  MAP_FILE //兼容标志，被忽略。
  MAP_32BIT //将映射区放在进程地址空间的低2GB，MAP_FIXED指定时会被忽略。当前这个标志只在x86-64平台上得到支持。
  MAP_POPULATE //为文件映射通过预读的方式准备好页表。随后对映射区的访问不会被页违例阻塞。
  MAP_NONBLOCK //仅和MAP_POPULATE一起使用时才有意义。不执行预读，只为已存在于内存中的页面建立页表入口。
 fd：有效的文件描述词。一般是由open()函数返回，其值也可以设置为-1，此时需要指定flags参数中的MAP_ANON,表明进行的是匿名映射。
 offset：被映射对象内容的起点
 */
int munmap(void *start, size_t length);
int mprotect(const void *start, size_t len, int prot);
/*把自start开始的、长度为len的内存区的保护属性修改为prot指定的值。
prot可以取以下几个值，并且可以用“|”将几个属性合起来使用：
1）PROT_READ：表示内存段内的内容可写；

2）PROT_WRITE：表示内存段内的内容可读；

3）PROT_EXEC：表示内存段中的内容可执行；

4）PROT_NONE：表示内存段中的内容根本没法访问。
*/
```
# 4. 动态链接与静态链接

下面编写一个简单的C程序，来看静态链接与动态链接的区别。
```c
#include<stdio.h>
int main(){

    while(1);
    return 0;
}

```
首先使用静态链接编译a.c程序生成a.out文件，然后使用动态链接生成b.out。
<pre><font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ gcc -static a.c </pre>
<pre><font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ gcc a.c -o b.out</pre>
可以看到静态链接的a.out的文件大小要远远大于动态链接的b.out。
<pre>-rwxr-xr-x 1 szp szp 845056 10月 24 16:56 <font color="#55FF55"><b>a.out</b></font>
-rwxr-xr-x 1 szp szp   8160 10月 24 16:58 <font color="#55FF55"><b>b.out</b></font>
</pre>
同时编译所用的时间，静态链接也会大于动态链接。
szp@szp-pc:~$ time gcc a.c -o b.out

real	0m0.063s
user	0m0.011s
sys	0m0.053s
szp@szp-pc:~$ time gcc -static a.c 

real	0m0.091s
user	0m0.071s
sys	0m0.020s

让两个程序都run起来，我们查看他们的虚存空间有什么不同。
先看静态链接程序的虚存空间。
<pre><font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ cat /proc/4103/maps
00400000-004b6000 r-xp 00000000 08:01 395781                             /home/szp/a.out（代码段）
006b6000-006bc000 rw-p 000b6000 08:01 395781                             /home/szp/a.out（数据段）
006bc000-006bd000 rw-p 00000000 00:00 0 （.bss）
0153c000-0155f000 rw-p 00000000 00:00 0                                  [heap]
7ffe2595b000-7ffe2597c000 rw-p 00000000 00:00 0                          [stack]
7ffe259f7000-7ffe259fa000 r--p 00000000 00:00 0                          [vvar]
7ffe259fa000-7ffe259fb000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
</pre>

第一行是代码段，第二行是数据段，第三行应该是bss，第四行是堆，第五行是栈。

动态链接的虚存空间。

<pre><font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ cat /proc/4114/maps
5556f7913000-5556f7914000 r-xp 00000000 08:01 395789                     /home/szp/b.out（代码段）
5556f7b13000-5556f7b14000 r--p 00000000 08:01 395789                     /home/szp/b.out
5556f7b14000-5556f7b15000 rw-p 00001000 08:01 395789                     /home/szp/b.out（数据段）
7fd257004000-7fd2571eb000 r-xp 00000000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7fd2571eb000-7fd2573eb000 ---p 001e7000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7fd2573eb000-7fd2573ef000 r--p 001e7000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7fd2573ef000-7fd2573f1000 rw-p 001eb000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7fd2573f1000-7fd2573f5000 rw-p 00000000 00:00 0 
7fd2573f5000-7fd25741e000 r-xp 00000000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7fd257607000-7fd257609000 rw-p 00000000 00:00 0 
7fd25761e000-7fd25761f000 r--p 00029000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7fd25761f000-7fd257620000 rw-p 0002a000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7fd257620000-7fd257621000 rw-p 00000000 00:00 0 
7ffe5779d000-7ffe577be000 rw-p 00000000 00:00 0                          [stack]
7ffe577f0000-7ffe577f3000 r--p 00000000 00:00 0                          [vvar]
7ffe577f3000-7ffe577f4000 r-xp 00000000 00:00 0                          [vdso]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
</pre>


可以看出动态链接程序多出了许多libc.so和ld.so。

接下来使用gdb命令对动态链接程序进行调试。
<pre><font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ cc -g a.c -o b.out
<font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ gdb b.out </pre>

使用starti命令，在程序执行第一条指令的时候让程序停下来，并在此时查看其虚存空间。

<pre>(gdb) starti
Starting program: /home/szp/b.out 

Program stopped.
0x00007ffff7dd4090 in _start () from /lib64/ld-linux-x86-64.so.2
(gdb) !cat /proc/4233/maps
555555554000-555555555000 r-xp 00000000 08:01 395789                     /home/szp/b.out
555555754000-555555756000 rw-p 00000000 08:01 395789                     /home/szp/b.out
7ffff7dd3000-7ffff7dfc000 r-xp 00000000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7ffff7ff8000-7ffff7ffb000 r--p 00000000 00:00 0                          [vvar]
7ffff7ffb000-7ffff7ffc000 r-xp 00000000 00:00 0                          [vdso]
7ffff7ffc000-7ffff7ffe000 rw-p 00029000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7ffff7ffe000-7ffff7fff000 rw-p 00000000 00:00 0 
7ffffffde000-7ffffffff000 rw-p 00000000 00:00 0                          [stack]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
</pre>

可以看到和刚才相比虚存空间中少了libc.so。其实libc是在程序执行的时候，使用ld.so（加载器）动态链接进来的。我们在mian处打一个断点，继续查看虚存空间。
<pre>(gdb) break main
Breakpoint 1 at 0x5555555545fe: file a.c, line 4.
(gdb) n
Single stepping until exit from function _start,
which has no line number information.

Breakpoint 1, main () at a.c:4
4	    while(1);
(gdb) !cat /proc/4233/maps
555555554000-555555555000 r-xp 00000000 08:01 395789                     /home/szp/b.out
555555754000-555555755000 r--p 00000000 08:01 395789                     /home/szp/b.out
555555755000-555555756000 rw-p 00001000 08:01 395789                     /home/szp/b.out
7ffff79e2000-7ffff7bc9000 r-xp 00000000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7ffff7bc9000-7ffff7dc9000 ---p 001e7000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7ffff7dc9000-7ffff7dcd000 r--p 001e7000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7ffff7dcd000-7ffff7dcf000 rw-p 001eb000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7ffff7dcf000-7ffff7dd3000 rw-p 00000000 00:00 0 
7ffff7dd3000-7ffff7dfc000 r-xp 00000000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7ffff7fe1000-7ffff7fe3000 rw-p 00000000 00:00 0 
7ffff7ff8000-7ffff7ffb000 r--p 00000000 00:00 0                          [vvar]
7ffff7ffb000-7ffff7ffc000 r-xp 00000000 00:00 0                          [vdso]
7ffff7ffc000-7ffff7ffd000 r--p 00029000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7ffff7ffd000-7ffff7ffe000 rw-p 0002a000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7ffff7ffe000-7ffff7fff000 rw-p 00000000 00:00 0 
7ffffffde000-7ffffffff000 rw-p 00000000 00:00 0                          [stack]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
</pre>

从程序的第一条指令到执行main函数这段过程中，发现libc.so已经被成功链接进来了。也就是说，动态链接程序是在程序运行的时候，将所需的库文件加载进虚存空间，所以编译后的程序比静态链接要小的多，而静态链接是在编译的时候就将所需的库文件打包到了一块，所以文件体积较大。

# 5. vdso(virtual dynamic shared object)

刚才查看了许多情况的虚存空间，其中有三个段vdso，vvar，vsyscall，存在于每个进程的虚存空间中，并且地址非常高。
由于系统调用陷入内核的代价非常大，操作系统提供了一种针对可读系统调用，无需陷入内核的功能。这段代码就在vdso段中，它是可读可执行的。
vvar：内核和进程共享的数据。
vdso：系统调用代码的实现。

可以看到操作系统实现了四个函数，可以不陷入内核执行系统调用。time函数会打印出从1970.1.1到今天所经过的秒数。下面调试一下time函数。
<pre><b>x86-64</b> <b>functions</b>
       The  table  below lists the symbols exported by the vDSO.  All of these symbols are also available without the &quot;__vdso_&quot;
       prefix, but you should ignore those and stick to the names below.

       symbol                 version
       ─────────────────────────────────
       __vdso_clock_gettime   LINUX_2.6
       __vdso_getcpu          LINUX_2.6
       __vdso_gettimeofday    LINUX_2.6
       __vdso_time            LINUX_2.6
</pre>
程序如下：
```c

#include<stdio.h>
int main(){

    printf("%d\n",time(0));
    return 0;

}
```
在main处打断点，运行程序，然后进入汇编模式。
<pre>(gdb) b main
Breakpoint 1 at 0x68e: file a.c, line 4.
(gdb) r
Starting program: /home/szp/a.out 

Breakpoint 1, main () at a.c:4
4	    printf(&quot;%d\n&quot;,time(0));
(gdb) layout asm
</pre>

<pre>   <span style="background-color:#00FF00"><font color="#000000"><b>┌────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐</b></font></span>
B+&gt;<span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span><span style="background-color:#00FF00"><font color="#000000">0x55555555468e &lt;main+4&gt;                 mov    $0x0,%edi</font></span>                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x555555554693 &lt;main+9&gt;                 mov    $0x0,%eax                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x555555554698 &lt;main+14&gt;                callq  0x555555554560 &lt;time@plt&gt;                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x55555555469d &lt;main+19&gt;                mov    %eax,%esi                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x55555555469f &lt;main+21&gt;                lea    0x9e(%rip),%rdi        # 0x555555554744                          <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546a6 &lt;main+28&gt;                mov    $0x0,%eax                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546ab &lt;main+33&gt;                callq  0x555555554550 &lt;printf@plt&gt;                                      <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546b0 &lt;main+38&gt;                mov    $0x0,%eax                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546b5 &lt;main+43&gt;                pop    %rbp                                                             <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546b6 &lt;main+44&gt;                retq                                                                    <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546b7                          nopw   0x0(%rax,%rax,1)                                                 <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546c0 &lt;__libc_csu_init&gt;        push   %r15                                                             <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546c2 &lt;__libc_csu_init+2&gt;      push   %r14                                                             <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546c4 &lt;__libc_csu_init+4&gt;      mov    %rdx,%r15                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x5555555546c7 &lt;__libc_csu_init+7&gt;      push   %r13                                                             <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>└────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘</b></font></span>
<span style="background-color:#00FF00"><font color="#000000">native process 4399 In: main                                                                L4    PC: 0x55555555468e </font></span>
(gdb) si
</pre>

输入si单步执行。
time调用了time@plt函数。

<pre>   <span style="background-color:#00FF00"><font color="#000000"><b>┌────────────────────────────────────────────────────────────────────────────────────────────────────────────────┐</b></font></span>
  &gt;<span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span><span style="background-color:#00FF00"><font color="#000000">0x7ffff7ffb931 &lt;time+1&gt;                 test   %rdi,%rdi</font></span>                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb934 &lt;time+4&gt;                 mov    -0x389b(%rip),%rax        # 0x7ffff7ff80a0                       <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb93b &lt;time+11&gt;                mov    %rsp,%rbp                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb93e &lt;time+14&gt;                je     0x7ffff7ffb943 &lt;time+19&gt;                                         <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb940 &lt;time+16&gt;                mov    %rax,(%rdi)                                                      <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb943 &lt;time+19&gt;                pop    %rbp                                                             <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb944 &lt;time+20&gt;                retq                                                                    <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb945                          nop                                                                     <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb946                          nopw   %cs:0x0(%rax,%rax,1)                                             <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb950 &lt;clock_gettime&gt;          push   %rbp                                                             <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb951 &lt;clock_gettime+1&gt;        cmp    $0xf,%edi                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb954 &lt;clock_gettime+4&gt;        mov    %rsp,%rbp                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb957 &lt;clock_gettime+7&gt;        push   %r12                                                             <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb959 &lt;clock_gettime+9&gt;        mov    %rsi,%r12                                                        <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>0x7ffff7ffb95c &lt;clock_gettime+12&gt;       push   %rbx                                                             <span style="background-color:#00FF00"><font color="#000000"><b>│</b></font></span>
   <span style="background-color:#00FF00"><font color="#000000"><b>└────────────────────────────────────────────────────────────────────────────────────────────────────────────────┘</b></font></span>
<span style="background-color:#00FF00"><font color="#000000">native process 4399 In: time                                                                L??   PC: 0x7ffff7ffb931 </font></span>
0x0000555555554560 in time@plt ()
(gdb) info inferiors
  Num  Description       Executable
* 1    process 4399      /home/szp/a.out
(gdb) si
0x00007ffff7ffb930 in time ()
</pre>

程序就跳转到了vdso段内地址，0x7ffff7ffb931是位于7ffff7ffb000-7ffff7ffc000内的。

<pre><font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ cat /proc/4399/maps
555555554000-555555555000 r-xp 00000000 08:01 395781                     /home/szp/a.out
555555754000-555555755000 r--p 00000000 08:01 395781                     /home/szp/a.out
555555755000-555555756000 rw-p 00001000 08:01 395781                     /home/szp/a.out
7ffff79e2000-7ffff7bc9000 r-xp 00000000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7ffff7bc9000-7ffff7dc9000 ---p 001e7000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7ffff7dc9000-7ffff7dcd000 r--p 001e7000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7ffff7dcd000-7ffff7dcf000 rw-p 001eb000 08:01 1080562                    /lib/x86_64-linux-gnu/libc-2.27.so
7ffff7dcf000-7ffff7dd3000 rw-p 00000000 00:00 0 
7ffff7dd3000-7ffff7dfc000 r-xp 00000000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7ffff7fe1000-7ffff7fe3000 rw-p 00000000 00:00 0 
7ffff7ff8000-7ffff7ffb000 r--p 00000000 00:00 0                          [vvar]
7ffff7ffb000-7ffff7ffc000 r-xp 00000000 00:00 0                          [vdso]
7ffff7ffc000-7ffff7ffd000 r--p 00029000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7ffff7ffd000-7ffff7ffe000 rw-p 0002a000 08:01 1080558                    /lib/x86_64-linux-gnu/ld-2.27.so
7ffff7ffe000-7ffff7fff000 rw-p 00000000 00:00 0 
7ffffffde000-7ffffffff000 rw-p 00000000 00:00 0                          [stack]
ffffffffff600000-ffffffffff601000 --xp 00000000 00:00 0                  [vsyscall]
</pre>
看接下来这条汇编指令，将%rip（当前指令）寄存器减去一个值，得到的内存地址赋值给%rax（函数返回值）寄存器，后面给出了注释，%rax的地址# 0x7ffff7ff80a0，而这个地址正是位于vvar段中。所以系统将时间从内存中某个位置拷贝到了vvar段中。操作系统通过这种共享内存的方式，为所有的进程提供了获取当前系统时间的系统调用。当然这个段只允许进程读，而不允许进程写，会触发段错误。
```asm
0x7ffff7ffb934 <time+4>                 mov    -0x389b(%rip),%rax        # 0x7ffff7ff80a0
```

还有最后一个vsyscall段，vsyscall中的指令只是简单调用了syscall系统调用，因为它是废弃的不陷入内核的系统调用方法，已经不再使用，为了向下兼容，保留了下来，并且让它直接调用syscall。

# 5. 静态链接实验

上面简单介绍了静态链接与动态链接，接下来通过一些实验来直观的看一下静态链接是如何实现的。

有如下两个程序a.c，b.c，a程序中调用了b程序中函数，通过这两个程序观察是a如何链接b的。

```c
//a.c
#include<stdio.h>
int fun(int x);
int main(){

    printf("%d\n",fun(0));
    return 0;

}
//b.c
#include<stdio.h>
int fun(int x){
    return x+1;
}

```
使用如下命令对程序a.c进行编译。
<pre><font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ gcc -o a.o -g -c -static a.c
</pre>

查看其对应的汇编代码。

<pre><font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ objdump -S -d a.o
a.o：     文件格式 elf64-x86-64


Disassembly of section .text:

0000000000000000 &lt;main&gt;:
#include&lt;stdio.h&gt;
int fun(int x);
int main(){
   0:	55                   	push   %rbp
   1:	48 89 e5             	mov    %rsp,%rbp

    printf(&quot;%d\n&quot;,fun(0));
   4:	bf 00 00 00 00       	mov    $0x0,%edi
   9:	e8 00 00 00 00       	callq  e &lt;main+0xe&gt;
   e:	89 c6                	mov    %eax,%esi
  10:	48 8d 3d 00 00 00 00 	lea    0x0(%rip),%rdi        # 17 &lt;main+0x17&gt;
  17:	b8 00 00 00 00       	mov    $0x0,%eax
  1c:	e8 00 00 00 00       	callq  21 &lt;main+0x21&gt;
    return 0;
  21:	b8 00 00 00 00       	mov    $0x0,%eax

}
  26:	5d                   	pop    %rbp
  27:	c3                   	retq   
</pre>


```asm
4:	bf 00 00 00 00       	mov    $0x0,%edi
9:	e8 00 00 00 00       	callq  e <main+0xe>
```
x86使用edi寄存器保存第一个参数的值，所以0x4处的指令后面的00 00 00 00应该是存放的变量x的值，它默认是初始化为0。而0x9处的指令应该是调用fun函数，后面的00 00 00 00 为fun函数的地址。因为该程序引用了一个外部的函数fun，当前并不知道fun函数会在哪里，所以编译器会预留位置，然后链接的时候对这些位置进行重填。那么链接器如何知道重填的位置呢？答案是存储在了elf文件中，链接器就是解析elf文件对这些位置进行重填。使用readelf，可以看到在elf文件中存储的应该重填的位置。

<pre><font color="#55FF55"><b>szp@szp-pc</b></font>:<font color="#5555FF"><b>~</b></font>$ readelf -r a.o

重定位节 &apos;.rela.text&apos; at offset 0xa48 contains 3 entries:
  偏移量          信息           类型           符号值        符号名称 + 加数
00000000000a  001000000004 R_X86_64_PLT32    0000000000000000 fun - 4
000000000013  000500000002 R_X86_64_PC32     0000000000000000 .rodata - 4
00000000001d  001100000004 R_X86_64_PLT32    0000000000000000 printf - 4
</pre>

可以看到fun函数重填的位置在0x00000000000a，也就是上面的0x9指令行的第二个位置。

# 总结

ELF文件中会有一个ELF header和若干个Program Header，每个Program Header都描述了需要将内存中的某一段映射成程序中的某一段。链接器就会负责解析ELF文件完成映射和地址的重定向。静态链接实验展示了这个过程。vdso机制提供了非陷入内核的系统调用，对于只读系统调用，减小了切换的开销。实现系统调用的关键，在于让内核知道某个进程想要进行系统调用，并且让程序能够知道哪里可以获取到结果。利用这种共享内存的方式，或许还可以实现更多的内核功能。

参考链接：https://www.bilibili.com/video/BV1N741177F5?p=15
