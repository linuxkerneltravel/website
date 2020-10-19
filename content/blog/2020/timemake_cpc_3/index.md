---
title: "小谈驱动开发中的时钟调试"
date: 2020-07-25T00:32:43+08:00
author: "作者：康华 编辑：崔鹏程"
keywords: ["驱动开发，时钟调试"]
categories : ["经验交流"]
banner : "img/blogimg/cpc3.jpg"
summary : "本文小谈了驱动开发中的时钟调试，先说了作者碰到的时钟问题，再说了如何调试时钟，是一篇值得阅读的文章"
---
## 小谈驱动开发中的时钟调试
  **――康华**

我想，调试过驱动的朋友多多少少都接触过时钟。因为时钟对设备来说，就如同心脏对人体供血一样，是不可或缺的。有些设备自己有时钟，有些接收系统外部时钟，总之时钟源是设备工作的基石。

因此检测时钟是否正工作是调试设备的关键之一，本文我将给出大家一个有效的时钟调试手段——不需要用试波器，而用软件方法。
     
我先说说我碰到的时钟问题，再来看如何调试时钟。
     
问题发生在开发mcc(多通道控制器)驱动时——在我们测试驱动时，发现无论如何配置mcc参数和触发设备，设备都自岿然不动，好似僵死一般。因此估计时钟为接收到。 经过调查，mcc所使用的时钟源mt9045没有设置对。 这个时钟源将给mcc输入传输、接收时钟，同时还有同步时钟。
      
由于当时我们缺乏足够的资料配置时钟，而且这个时钟的确比较复杂，所以我们忙了好一阵都没搞通它。 因此我们急需检测时钟输入的正确与否。如果用试波器吧，还阵不知道如何接出硬件管脚（设备封装得很好），最后我们借助一段软件程序来检测端口时钟频率。
 

```c
static int frequency_count(volatile unsigned long  addr, unsigned int mask )
{
    int t, nt, et;

    int v, lv, cnt;
    
    t = Gettick();
    while ( 1 )
    {
        nt = Gettick();
        if ( nt != t )
            break;
    }
    et = nt + GetsysClkRate ();
    cnt = 0;
    lv = v = (*(int*)addr) & mask;
    while ( 1 )
    {
        nt = Gettick();
        if ( nt >= et )
            break;
        v = (*(int*)addr) & mask;
        if ( v != lv )
        {
            lv = v;
            cnt++;
        }
    }
    return cnt/2;
}
```
上面程序的计算值是近似的――当系统cpu频率约快时，准确性约搞――其中机理留给大家去领会。

函数的用法很简单。当检测某个时钟输出，即特定地址线上的某个针（软件角度就一个位）是否产生何时的频率时，只需要给frequency_count 传入地址地址和输出针的掩码，如地址0xfdf90d50的第0位，则传入0xfdf90d50,0x00000001。 函数输出就时频率值。
       
要注意其中有两个辅助函数。一个是Gettick ，它用于获取系统当前节拍值；一个是GetsysClkRate用于获得系统节拍频率。
      
在Linux系统中上述函数可如下实现。
```c
static int Gettick()
{
   return (int)jiffies;
}

static int GetsysClkRate ()
{
   return (int)HZ;
}

```


对vxWorks系统上述函数可实现如下
 

```c
static int Gettick()
{
   return tickGet();
}
static int GetsysClkRate ()
{
   return sysClkRateGet();
}
```
