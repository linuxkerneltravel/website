---
title: "proc文件系统探索 之 proc根目录下的文件[七]"
date: 2020-09-29T21:21:25+08:00
author: "吴刚，陈小龙转"
keywords: ["proc","root"]
categories : ["走进内核"]
banner : "img/blogimg/9.jpg"
summary : "主要参考内核文档和红帽文档对 > cat /proc/meminfo   读出的内核信息进行解释， 下篇文章会简单对读出该信息的代码进行简单的分析。"
---

主要参考内核文档和红帽文档对 > cat /proc/meminfo  读出的内核信息进行解释， 下篇文章会简单对读出该信息的代码进行简单的分析。

```
MemTotal: 507480 kB MemFree: 10800 kB Buffers: 34728 kB Cached: 98852 kB SwapCached: 128 kB Active: 304248 kB Inactive: 46192 kB HighTotal: 0 kB HighFree: 0 kB LowTotal: 507480 kB LowFree: 10800 kB SwapTotal: 979956 kB SwapFree: 941296 kB Dirty: 32 kB Writeback: 0 kB AnonPages: 216756 kB Mapped: 77560 kB Slab: 22952 kB SReclaimable: 15512 kB SUnreclaim: 7440 kB PageTables: 2640 kB NFS_Unstable: 0 kB Bounce: 0 kB CommitLimit: 1233696 kB Committed_AS: 828508 kB VmallocTotal: 516088 kB VmallocUsed: 5032 kB VmallocChunk: 510580 kB
```

相应选项中文意思想各位高手已经知道，如果翻译有什么错误，请务必指出：

 MemTotal: 所有可用RAM大小 （即物理内存减去一些预留位和内核的二进制代码大小）

 MemFree: LowFree与HighFree的总和，被系统留着未使用的内存 

Buffers: 用来给文件做缓冲大小 

Cached: 被高速缓冲存储器（cache memory）用的内存的大小（等于 diskcache minus SwapCache ）. SwapCached:被高速缓冲存储器（cache memory）用的交换空间的大小已经 被交换出来的内存，但仍然被存放在swapfile中。用来在需要的时候很快的 被替换而不需要再次打开I/O端口。

 Active: 在活跃使用中的缓冲或高速缓冲存储器页面文件的大小，除非非常必要否则不会被移作他用. Inactive: 在不经常使用中的缓冲或高速缓冲存储器页面文件的大小，可能被用于其他途径. 

HighTotal: HighFree: 该区域不是直接映射到内核空间。内核必须使用不同的手法使用该段内存。

 LowTotal: LowFree: 低位可以达到高位内存一样的作用，而且它还能够被内核用来记录 一些自己的数据结构。

Among many other things, it is where everything from the Slab is allocated. Bad things happen when you're out of lowmem. 

SwapTotal: 交换空间的总大小 SwapFree: 未被使用交换空间的大小 Dirty: 等待被写回到磁盘的内存大小。 Writeback: 正在被写回到磁盘的内存大小。 AnonPages：未映射页的内存大小 Mapped: 设备和文件等映射的大小。

 Slab: 内核数据结构缓存的大小，可以减少申请和释放内存带来的消耗。

 SReclaimable:可收回Slab的大小 

SUnreclaim：不可收回Slab的大小（SUnreclaim+SReclaimable＝Slab）

 PageTables：管理内存分页页面的索引表的大小。 

NFS_Unstable:不稳定页表的大小 Bounce: CommitLimit: Based on the overcommit ratio('vm.overcommit_ratio'), this is the total amount of memory currently available to be allocated on the system. This limit is only adhered to if strict overcommit accounting is enabled (mode 2 in 'vm.overcommit_memory'). The CommitLimit is calculated with the following formula: CommitLimit = ('vm.overcommit_ratio' * Physical RAM) + Swap For example, on a system with 1G of physical RAM and 7G of swap with a `vm.overcommit_ratio` of 30 it would yield a CommitLimit of 7.3G. For more details, see the memory overcommit documentation in vm/overcommit-accounting. Committed_AS: The amount of memory presently allocated on the system. The committed memory is a sum of all of the memory which has been allocated by processes, even if it has not been "used" by them as of yet. A process which malloc()'s 1G of memory, but only touches 300M of it will only show up as using 300M of memory even if it has the address space allocated for the entire 1G. This 1G is memory which has been "committed" to by the VM and can be used at any time by the allocating application. With strict overcommit enabled on the system (mode 2 in 'vm.overcommit_memory'), allocations which would exceed the CommitLimit (detailed above) will not be permitted. This is useful if one needs to guarantee that processes will not fail due to lack of memory once that memory has been successfully allocated. VmallocTotal: 可以vmalloc虚拟内存大小 VmallocUsed: 已经被使用的虚拟内存大小。 VmallocChunk: largest contigious block of vmalloc area which is free 下面简单来个例子，看看已用内存和物理内存大小..

`#include <stdio.h>`

 #include <stdlib.h>

#include <string.h> 

int MemInfo(char* Info, int len);

 int main() { char buf[128]; 

memset(buf, 0, 128); 

MemInfo(buf, 100); 

printf("%s", buf); return 0; }

 int MemInfo(char* Info, int len) { 

char sStatBuf[256]; FILE* fp;

 int flag; int TotalMem;

 int UsedMem; 

char* line; 

if(system("free -m | awk '{print $2,$3}' > mem")); memset(sStatBuf, 0, 256); 

fp = fopen("mem", "rb"); if(fp < 0) { return -1; } 

fread(sStatBuf,1, sizeof(sStatBuf) , fp);` line = strstr(sStatBuf, "\n");

 TotalMem = atoi(line); line = strstr(line, " "); 

UsedMem = atoi(line); memset(sStatBuf, 0, 256);

 sprintf(sStatBuf, "Used %dM/Total %dM\n", UsedMem, TotalMem); 

if(strlen(sStatBuf) > len) { return -1; } 

memcpy(Info, sStatBuf, strlen(sStatBuf)); return 0; } 

结果：Used 488M/Total 495M

上面文章对meminfo里的信息做出简单的解释了
那么内核怎么把meminfo信息动态反应到meminfo文件中呢
在内核 linux/fs/proc/proc_misc.c中

`static int meminfo_read_proc(char *page, char **start, off_t off, int count, int *eof, void *data) { struct sysinfo i; int len; unsigned long committed; unsigned long allowed; struct vmalloc_info vmi; long cached;` 

#define K(x) ((x) << (PAGE_SHIFT - 10)) /** *该宏作用把存储单位传换成 kb */ si_meminfo(&i); si_swapinfo(&i); /** *这两个函数是对struct sysinfo结构进行初始化的 */ 

committed = atomic_read(&vm_committed_space);

 allowed = ((totalram_pages - hugetlb_total_pages()) * sysctl_overcommit_ratio / 100) + total_swap_pages; /** *其中这项根据上篇文章CommitLimit解释计算的 */*

ached = global_page_state(NR_FILE_PAGES) - total_swapcache_pages - i.bufferram;

 if (cached < 0) cached = 0; 

get_vmalloc_info(&vmi); /* * Tagged format, for easy grepping and expansion. */

 len = sprintf(page, "MemTotal: %8lu kB\n" "MemFree: %8lu kB\n" "Buffers: %8lu kB\n" "Cached: %8lu kB\n" "SwapCached: %8lu kB\n" "Active: %8lu kB\n" "Inactive: %8lu kB\n" #ifdef CONFIG_HIGHMEM "HighTotal: %8lu kB\n" "HighFree: %8lu kB\n" "LowTotal: %8lu kB\n" "LowFree: %8lu kB\n" #endif "SwapTotal: %8lu kB\n" "SwapFree: %8lu kB\n" "Dirty:   %8lu kB\n" "Writeback: %8lu kB\n" "AnonPages: %8lu kB\n" "Mapped: %8lu kB\n" "Slab:    %8lu kB\n" "SReclaimable: %8lu kB\n" "SUnreclaim: %8lu kB\n" "PageTables: %8lu kB\n" "NFS_Unstable: %8lu kB\n" "Bounce: %8lu kB\n" "CommitLimit: %8lu kB\n" "Committed_AS: %8lu kB\n" "VmallocTotal: %8lu kB\n" "VmallocUsed: %8lu kB\n" "VmallocChunk: %8lu kB\n", K(i.totalram), K(i.freeram), K(i.bufferram), K(cached), K(total_swapcache_pages), K(global_page_state(NR_ACTIVE)), K(global_page_state(NR_INACTIVE)), 

#ifdef CONFIG_HIGHMEM K(i.totalhigh), K(i.freehigh), K(i.totalram-i.totalhigh), K(i.freeram-i.freehigh), #endif K(i.totalswap), K(i.freeswap), K(global_page_state(NR_FILE_DIRTY)), K(global_page_state(NR_WRITEBACK)), K(global_page_state(NR_ANON_PAGES)), K(global_page_state(NR_FILE_MAPPED)), K(global_page_state(NR_SLAB_RECLAIMABLE) + global_page_state(NR_SLAB_UNRECLAIMABLE)), K(global_page_state(NR_SLAB_RECLAIMABLE)), K(global_page_state(NR_SLAB_UNRECLAIMABLE)), K(global_page_state(NR_PAGETABLE)), K(global_page_state(NR_UNSTABLE_NFS)), K(global_page_state(NR_BOUNCE)), K(allowed), K(committed), (unsigned long)VMALLOC_TOTAL >> 10, vmi.used >> 10, vmi.largest_chunk >> 10 );

 len += hugetlb_report_meminfo(page + len); 

return proc_calc_metrics(page, start, off, count, eof, len); 

#undef K }

其中sysinfo结构在 linux/kernel.h 定义：

```
struct sysinfo { 
long uptime; /* 启动到现在经过的时间 */ 
unsigned long loads[3]; /* 1, 5, and 15 minute load averages */ 
unsigned long totalram; /* 总的可用的内存大小 */ 
unsigned long freeram; /* 还未被使用的内存大小 */ 
unsigned long sharedram; /* 共享的存储器的大小*/
unsigned long bufferram; /* 的存储器的大小 */ 
unsigned long totalswap; /* 交换区大小 */ 
unsigned long freeswap; /* 还可用的交换区大小 */
unsigned short procs; /* 当前进程数目 */ 
unsigned short pad; /* explicit padding for m68k */
unsigned long totalhigh; /* 总的高内存大小 */ 
unsigned long freehigh; /* 可用的高内存大小 */ 
unsigned int mem_unit; /* 以字节为单位的内存大小 */ 
char _f[20-2*sizeof(long)-sizeof(int)]; };
```

而global_page_state()函数中的常量定义在 linux/mmzone.h



```
enum zone_stat_item {
/* First 128 byte cacheline (assuming 64 bit words) */ 
NR_FREE_PAGES, NR_INACTIVE, NR_ACTIVE, NR_ANON_PAGES, /* Mapped anonymous pages */ NR_FILE_MAPPED, /* pagecache pages mapped into pagetables. only modified from process context */ 
NR_FILE_PAGES, NR_FILE_DIRTY, NR_WRITEBACK, /* Second 128 byte cacheline */ NR_SLAB_RECLAIMABLE, NR_SLAB_UNRECLAIMABLE, NR_PAGETABLE, /* used for pagetables */ NR_UNSTABLE_NFS, /* NFS unstable pages */ 
NR_BOUNCE, NR_VMSCAN_WRITE, 
#ifdef CONFIG_NUMA NUMA_HIT, /* allocated in intended node */ 
NUMA_MISS, /* allocated in non intended node */
NUMA_FOREIGN, /* was intended here, hit elsewhere */ 
NUMA_INTERLEAVE_HIT, /* interleaver preferred this zone */ 
NUMA_LOCAL, /* allocation from local node */
NUMA_OTHER, /* allocation from other node */ #endif NR_VM_ZONE_STAT_ITEMS };
```

其中通过global_page_state()函数根据 zone_stat_item 结构的常量得到不同区大小，会跟 vm_stat[NR_VM_ZONE_STAT_ITEMS]对应起来。
vm_stat[]是统计各区的大小。

内核 linux/vmstat.h定义：

static inline unsigned long global_page_state(enum zone_stat_item item) { 

`long x = atomic_long_read(&vm_stat[item]);

 #ifdef CONFIG_SMP if (x < 0) x = 0; #endif return x; }` 

下面根据struct sysinfo结构，简单分析CPU和内存使用信息。

 #include <stdio.h> 

#include <linux/unistd.h> /* 包含调用 _syscallX 宏等相关信息*/ *

*#include <linux/kernel.h> /* 包含sysinfo结构体信息*/ 

int main(int argc, char *agrv[]) { 

struct sysinfo s_info; int error; error = sysinfo(&s_info); 

printf("code error=%d\n",error); 

printf("Uptime = %ds\nLoad: 1 min%d / 5 min %d / 15 min %d\n" "RAM: total %d / free %d /shared%d\n" "Memory in buffers = %d\nSwap:total%d/free%d\n" "Number of processes = %d\n", s_info.uptime, s_info.loads[0], s_info.loads[1], s_info.loads[2], s_info.totalram, s_info.freeram, s_info.totalswap, s_info.freeswap, s_info.procs); return 0; } 

结果： code error=0 Uptime = 8329s Load: 1 min37152 / 5 min 37792 / 15 min 48672 RAM: total 519659520 / free 9031680 /shared1003474944 Memory in buffers = 937451520 Swap:total223/free-1078732672 Number of processes = -1078732608

