---
title: "proc文件系统探索 之 以数字命名的目录[四]"
date: 2020-12-19T10:38:13+08:00
author: "孙张品转"
keywords: ["proc"]
categories : ["走进内核"]
banner : "img/blogimg/cxl_11.jpg"
summary : "statm文件描述进程的内存状态。下面我们来详细解释该文件中内容的含义。"
---

10．statm文件 描述进程的内存状态。
```
niutao@niutao-desktop:/proc/6950$ cat statm 12992 4432 3213 144 0 1028 0 niutao@niutao-desktop:/proc/6950$
```
下面我们来详细解释该文件中内容的含义。首先我们可以在内核中搜索到该文件的内容是由函数proc_pid_statm()函数写入的：（/fs/proc/array.c）
```c
int proc_pid_statm(struct task_struct *task, char *buffer) { 
    int size = 0, resident = 0, shared = 0, text = 0, lib = 0, data = 0; 
    struct mm_struct *mm = get_task_mm(task); 
    if (mm) { 
        size = task_statm(mm, &shared, &text, &data, &resident); 
        mmput(mm); 
    } 
    return sprintf(buffer, "%d %d %d %d %d %d %d\n", size, resident, shared, text, lib, data, 0); 
} /*fs/proc/task_mmu.c*/ 
int task_statm(struct mm_struct *mm, int *shared, int *text, int *data, int *resident) { 
    *shared = get_mm_counter(mm, file_rss);
    *text = (PAGE_ALIGN(mm->end_code) - (mm->start_code & PAGE_MASK)) >> PAGE_SHIFT; 
    *data = mm->total_vm - mm->shared_vm; 
    *resident = *shared + get_mm_counter(mm, anon_rss); return mm->total_vm; 
}
```

size表示进程虚拟地址空间的大小（单位为 页），resident表示文件映射内存大小和分配给匿名内存映射的大小（单位为页），shared表示共享文件内存映射大小（单位为页），text表示 可执行代码区域的内存空间的大小（单位为页），所以该进程的内存信息可描述为其虚拟地址空间的大小为12992页(将近60MB)，文 件映射内存大小和分配给匿名内存映射的大小为4432页（将近18MB）. 

11．status文件： 用可读的方式描述进程的状态
```
niutao@niutao-desktop:/proc/9744$ cat status Name: gedit /*进程的程序名*/ 

State: S (sleeping) /*进程的状态信息,具体参见http://blog.chinaunix.net/u2/73528/showart_1106510.html*/ 
Tgid: 9744 /*线程组号*/ 
Pid: 9744 /*进程pid*/ 
PPid: 7672 /*父进程的pid*/ 
TracerPid: 0 /*跟踪进程的pid*/ 
Uid: 1000    1000    1000    1000 /*uid euid suid fsuid*/ 
Gid: 1000    1000    1000    1000 /*gid egid sgid fsgid*/ 
FDSize: 256 /*文件描述符的最大个数，file->fds*/ 
Groups: 0 4 20 24 25 29 30 44 46 107 109 115 124 1000 /*启动该进程的用户所属的组的id*/ VmPeak: 60184 kB /*进程地址空间的大小*/ 
VmSize: 60180 kB /*进程虚拟地址空间的大小reserved_vm：进程在预留或特殊的内存间的物理页*/ 
VmLck: 0 kB /*进程已经锁住的物理内存的大小.锁住的物理内存不能交换到硬盘*/ 
VmHWM: 18020 kB /*文件内存映射和匿名内存映射的大小*/ 
VmRSS: 18020 kB /*应用程序正在使用的物理内存的大小，就是用ps命令的参数rss的值 (rss)*/ 
VmData: 12240 kB /*程序数据段的大小（所占虚拟内存的大小），存放初始化了的数据*/ 
VmStk: 84 kB /*进程在用户态的栈的大小*/ VmExe: 576 kB /*程序所拥有的可执行虚拟内存的大小,代码段,不包括任务使用的库 */ 
VmLib: 21072 kB /*被映像到任务的虚拟内存空间的库的大小*/ 
VmPTE: 56 kB /*该进程的所有页表的大小*/ 
Threads: 1 /*共享使用该信号描述符的任务的个数*/ 
SigQ: 0/8183 /*待处理信号的个数/目前最大可以处理的信号的个数*/ 
SigPnd: 0000000000000000 /*屏蔽位，存储了该线程的待处理信号*/ 
ShdPnd: 0000000000000000 /*屏蔽位，存储了该线程组的待处理信号*/ 
SigBlk: 0000000000000000 /*存放被阻塞的信号*/ 
SigIgn: 0000000000001000 /*存放被忽略的信号*/ 
SigCgt: 0000000180000000 /*存放被俘获到的信号*/ 
CapInh: 0000000000000000 /*能被当前进程执行的程序的继承的能力*/ 
CapPrm: 0000000000000000 /*进程能够使用的能力，可以包含CapEff中没有的能力，这些能力是被进程自己临时放弃的*/ 
CapEff: 0000000000000000 /*是CapPrm的一个子集，进程放弃没有必要的能力有利于提高安全性*/ Cpus_allowed: 01 /*可以执行该进程的CPU掩码集*/ 
Mems_allowed: 1 /**/ 
voluntary_ctxt_switches: 1241 /*进程主动切换的次数*/ 
nonvoluntary_ctxt_switches: 717 /*进程被动切换的次数*/
```

该文件的内容在内核中由proc_pid_status函数写入：
```c
int proc_pid_status(struct task_struct *task, char *buffer) { 
    char *orig = buffer; struct mm_struct *mm = get_task_mm(task);
     buffer = task_name(task, buffer);
      buffer = task_state(task, buffer); 
      if (mm) { 
          buffer = task_mem(mm, buffer); 
          mmput(mm); 
    } 
    buffer = task_sig(task, buffer); 
    buffer = task_cap(task, buffer); 
    buffer = cpuset_task_status_allowed(task, buffer); 
    #if defined(CONFIG_S390) 
    buffer = task_show_regs(task, buffer); 
    #endif buffer = task_context_switch_counts(task, buffer); 
    return buffer - orig; 
}
```
经过以上分析，我们知道该进程的程序名为gedit，目前处 于睡眠状态，该进程的线程组号为9744，进程的pid为9744，父进程的pid为7672，没有跟踪进程。该进程所属用户的id为1000，用户组 id为1000，限制该进程最大可以同时打开256个文件。进程的地址空间的大小是60184 kB，进程的虚拟地址空间大小是60180 kB，常驻物理内存的大小为0KB，文件内存映射和匿名内存映射的大小为18020 kB，程序正在使用的物理内存的大小18020 kB，程序数据段的大小12240 kB，进程在用户态的栈的大小84KB，程序所拥有的可执行虚拟内存的大小576KB，被映像到进程的虚拟内存空间的库的大小21072KB，该进程的 所有页表的大小56KB，只有一个进程共享使用该进程的信号描述符，没有带处理的信号，进程主动切换了1241次，被动切换了717次。 

12．mounts文件 该文件包含该系统挂在的文件系统的信息。该文件在/proc下和每个进程文件夹下都有，并且内容一样。

```
niutao@niutao-desktop:/proc/1$ cat mounts rootfs / rootfs rw 0 0 none /sys sysfs rw,nosuid,nodev,noexec 0 0 none /proc proc rw,nosuid,nodev,noexec 0 0 udev /dev tmpfs rw,relatime 0 0 fusectl /sys/fs/fuse/connections fusectl rw,relatime 0 0 /dev/disk/by-uuid/f9f21592-a8a3-4e61-ac3d-0c7b7aa2cd42 / ext3 rw,relatime,errors=remount-ro,data=ordered 0 0 /dev/disk/by-uuid/f9f21592-a8a3-4e61-ac3d-0c7b7aa2cd42 /dev/.static/dev ext3 rw,relatime,errors=remount-ro,data=ordered 0 0 ....
```

该文件的输出结果和/etc/mtab文件的内容类似，但比/etc/mtab文件多一些内容。第一列指出被挂载的设备，第二列表示挂载点，第三列指出该文 件系统的类型。第四列对该挂载的文件系统的读写权限，一般有ro(read-only )和rw(read-write)。第五列和第六列是虚拟数据，用在/etc/mtab中。 

13．io文件

```

niutao@niutao-desktop:/proc/1$ cat io rchar: 14699 /*task_struct->rchar*/ wchar: 20553017 /*task_struct->wchar*/ syscr: 350 /*task_struct->syscr*/ syscw: 1128 /*task_struct->syscw,*/ read_bytes: 1605632 /*task_struct->ioac.read_bytes*/ write_bytes: 0 /*task_struct->ioac.write_bytes*/ cancelled_write_bytes: 0 /*task_struct->ioac.cancelled_write_bytes*/.

```