---
title: "虚拟机中GUEST OS时钟(TIMEKEEP)问题的探讨"
date: 2020-08-11T17:46:44+08:00
author: "作者：康华 编辑：张孝家"
keywords: ["时钟"]
categories : ["经验交流"]
banner : "img/blogimg/zxj0.jpg"
summary : "操作系统的时钟处理按理来说应该是个早已成熟的技术,不必再费口舌讨论什么。事实也的确如此。然而在虚拟环境下(不仅仅是xen,vmware这些虚拟机)，对时钟的处理可绝非轻而易举，如果你耐心看看你虚拟环境下运行的guest操作系统，如linux/windows等,往往用眼睛就可发觉其wall clock走的不是那么的准确,有些飘忽,时快时慢. 如果你运行某些性能测试工具,也会发现测试数据忽高忽低。这是为什么呢? 怎么解决呢? 本文就该问题进行一些初步探讨."
---

操作系统的时钟处理按理来说应该是个早已成熟的技术,不必再费口舌讨论什么。事实也的确如此。然而在虚拟环境下(不仅仅是xen,vmware这些虚拟机)，对时钟的处理可绝非轻而易举，如果你耐心看看你虚拟环境下运行的guest操作系统，如linux/windows等,往往用眼睛就可发觉其wall clock走的不是那么的准确,有些飘忽,时快时慢. 如果你运行某些性能测试工具,也会发现测试数据忽高忽低。这是为什么呢? 怎么解决呢? 本文就该问题进行一些初步探讨.

## 操作系统中的时钟源

先来谈谈北京知识:操作系统的时钟源吧. 目前x86体系的操作系统中使用的时钟源有

RTC , TSC ,PIT , CPU Local Timer , HPET , ACPI Power Management Timer 。其中pit 是操作系统传统上使用的timekeeping 时钟,其精度不是很高，因此很多操作系统(如vista/Linux)目前都使用或支持使用RTC中的周期性定时器来做timekeeping的时钟源。TSC不能产生中断,只能读取，因而多用来配合其它时钟做些校正等工作。至于其它几个时钟也可用timekeeping,其中各自的特点请大家参看<<understanding the linux kernel >>中Timing Measurements一章。本文不展开了讲述了。

我们再来谈谈timekeep吧！操作系统timekeeping体系大致包含下面几个部分：

1.  更新系统启动后的时间流失
2. 更新wall clock
3. 计算进程的时间片
4. 更新资源计数
5. 处理软件定时器

我们上面说的wall clock不准,问题主要就出在”更新wall clock”这一过程中。该过程其实很简单,就是在每次时钟中断时刻更新系统的wall clock。然而要想wall clock准确，必须基于一个事实――系统的时钟中断准确送上来，且能及时被处理。别看这么简单，在传统环境下它不是个问题，然后在虚拟环境下可很难保证这个事实。因为传统的抢占式中断处理过程在虚拟环境下变成了类似信号处理的延迟执行方式了。

更确切的说就是虚拟环境下 1 模拟的时钟中断不能保证精确的按时发出,它有可能延迟；2 时钟中断的处理并非”抢占”式,只有到guest os获得运行时刻才可执行。上述两点便是时钟问题的根本症结。

## XEN虚拟机中的时钟

xen的架构我这里不介绍了,如果不熟悉的话,你一定先要找资料研究一番,因为下面的内容和xen架构密切相关.

本文所描述的虚拟环境可见下图.其中有几点要说明一下:在Domain0(也被称作SOS)中紫色模块之一是 Device Mode, 硬件相关的模拟都由其完成(时钟也是),它运行于用户空间; 最右边方框代表支持VMX(就是intel的VT技术)的guset os (虽然图中画的是Linux,但实际上也可以运行windows系统) ;最下面是运行于真实硬件上的xen.

Xen虚拟环境的运行机制下, GUEST OS能使用的时钟来自于Device Mode的模拟,而软件时钟中断的发现和处理时机则是每次发生VM Exit/Vm Entry的时刻.如果将该时刻比作进程调度的话,这点很象进程发现信号的过程.

虚拟环境中系统时钟是使用软件模拟的,即使用软件模拟时钟中断. 如果看看xen3.0.3的代码你会发现其定义了pl_time结构以描述rtc,pit,pmt三种系统时钟(或叫平台时钟),这三个时钟都是使用软件定时器进行模拟.(相关代码可以在目录ioemu中发现).

就xen结构而言,我们知道多数设备的模拟都处于运行于用户空间的qemu程序进行模拟,时钟的模拟最初也是如此. 可是这样做存在一个隐蔽的问题 ---- 时钟模拟程序在某些情况下无法保证被及时调度,从而也就无法保证能及时准确的模拟时钟中断.

所谓的某些情况包括: 1 SOS(qmeu运行于SOS的用户空间)负载重,因而影响qemu被调度的机会,也就使得软件时钟中断无能及时产生----模拟代码没有机会调度自然无法产生软件时钟中断 2 模拟时钟的频率要求过高, 造成SOS无法模拟(如果模拟时钟的频率要求高于SOS的硬件时钟中断频率,则就无法模拟,比如SOS最高提供1000hz的时钟源,那么qmeu可模拟的软件时钟中断则不会超过1000hz) ,除了上述两条外, 高频时钟模拟也给系统带来很大的负载,从而影响虚拟机的整体性能.

## 解决办法

  我们分析了虚拟环境下GUEST OS时钟不准的原因,那么有什么方法可以解决或减轻该该问题呢?

  解决时钟问题可从如下几个方面进行.    

  1 将模拟时钟从SOS的qemu中移植到xen. 这样做可更好保证时钟模拟程序可得到运行.因为在xen中运行机会要比在qemu中更能保证 (xen3.0.4已经这么做了)

  2 对时钟软件中断进行记录,并在Guest os从xen返回时刻将丢失的软件中断补回去，从而防止时钟中断丢失.具体做法是每次软件定时器模拟出时钟中断时都计算是否有丢失的中断----***丢失的软件时钟中断数目 ==\*** ***（当前时刻－定时期上次发送软件时钟中断时刻）/\******时钟周期\***。在VM entry时，如果有丢失中断则将其发送给Guest OS. 注意这里不用担心VM entry发生频率太低，不足以将丢失的时钟中断送上去，因为VM exit/ＶＭ entry的发生频率大概每秒都数万次,绝对比时钟中断频率高,因此不会丢失时钟中断,这样补偿方式很保险,大概每天时钟也就慢几秒.

  3 在GUEST OS中调整wall clock, 这个做法很简单就是修改Guest OS的内核,做一个守护进程,让其不断修正wall clock,修正的依据可通过读取xen的时钟,如果发现慢于xen的时钟则将自己的wall clock时钟按照xen时间设置.

## 进一步思索

从上面情况分析的情况来看利用软件时钟中断加上补偿技术,Guest OS系统的wall clock可以保证准确,至少对人的视觉来说是足够的精确了. **时钟模拟的不足在于:**　**1** **模拟高频率时钟将给xen****一定的负载,****降低系统性能. 2** **时钟中断只能利用vm exit/entry****时机才可注入到Guest Os,****因此并不能向硬件那样精确的发上,****这点限制了虚拟机在很多实时环境的应用.**

对于第一个缺点,也许可以回避,具体的做法是不考软件模拟时钟,而是使用真实时钟来产生时钟中断,送给Guest OS. 举个例子,32位的smp windows系统使用rtc时钟作为time keeping的时钟源,那么我们就使用真实的RTC设备来产生中断,而不再利用模拟硬件方式来做,这样以来就解放了cpu模拟时钟中断的负担了。

先来说一下RTC时钟和设备模拟原理,然后我们看看如何利用真实RTC中断代替模拟时钟中断. RTC时钟(具体硬件手册见附录)的模拟原理实质上是 :　以内存存储替代寄存器和coms的ROM(可参见RTCState结构,该结构描述了RTC各种状态)　; 以软件定时器模拟RTC的三个硬件定时器. 当Guest OS 访问RTC控制端口时,系统发生vm exit,控制权便由guest os 交给xen.此刻RTC模拟程序(rtc_ioport_write)会根据访问的控制端口,模拟相应的动作.比如写端口0x0a设置定时器频率,然后写端口0x0b使能定时器,这个硬件动作可以通过如下软件模拟(见tools/ioemu/hw/mc146818rtc.c或xen/arch/x86/hvm/rtc.c)
```
case RTC_REG_A:
            /* UIP bit is read only */
            s->cmos_data[RTC_REG_A] = (data & ~RTC_UIP) |
                (s->cmos_data[RTC_REG_A] & RTC_UIP);
            rtc_timer_update(s);
            break;
case RTC_REG_B:
            if (data & RTC_SET) {
                /* set mode: reset UIP mode */
                s->cmos_data[RTC_REG_A] &= ~RTC_UIP;
                data &= ~RTC_UIE;
            } else {
                /* if disabling set mode, update the time */
                if (s->cmos_data[RTC_REG_B] & RTC_SET) {
                    rtc_set_time(s);
                }
            }
            s->cmos_data[RTC_REG_B] = data;
            rtc_timer_update(s);
```
现在在看看我们如何使用真实rtc发射时钟中断。真实发射中断需要对真实rtc的控制寄存器进行写操作，所以要做的只是用真实操作rtc寄存器，替换掉上述代码的模拟动作部分。修改后代码如下（见
```
rtc_ioport_write)：
	case RTC_REG_A:
             /* UIP bit is read only */
             s->cmos_data[RTC_REG_A] = (data & ~REG_A_UIP) |
                 (s->cmos_data[RTC_REG_A] & REG_A_UIP);
             //rtc_timer_update(s, qemu_get_clock(vm_clock));
             iopl(3);
             outb(0x0a,0x70);
             outb(s->cmos_data[RTC_REG_A],0x71);
             break;
	case RTC_REG_B:
            if (data & RTC_SET) {
                /* set mode: reset UIP mode */
                s->cmos_data[RTC_REG_A] &= ~RTC_UIP;
                data &= ~RTC_UIE;
            } else {
                /* if disabling set mode, update the time */
                if (s->cmos_data[RTC_REG_B] & RTC_SET) {
                    rtc_set_time(s);
                }
            }
             s->cmos_data[RTC_REG_B] = data;
            //rtc_timer_update(s, qemu_get_clock(vm_clock));
             iopl(3);
             outb(0xb,0x70);
             outb(s->cmos_data[RTC_REG_B],0x71);
             break;
```

同样对状态寄存器C的读操作(rtc_ioport_read)也要做相应处理：

```
         case RTC_REG_C:
            iopl(3);
            outb(0xc,0x70);
            ret= inb(0x71);
           // ret = s->cmos_data[s->cmos_index];
           // pic_set_irq(s->irq, 0);
            s->cmos_data[RTC_REG_C] = 0x00;
            break;
         default:
             ret = s->cmos_data[s->cmos_index];
             break;
         }
```
最后别忘了注释掉RTCState *rtc_init(int base, int irq)中的s->periodic_timer = qemu_new_timer(vm_clock, rtc_periodic_timer, s); 一句，因为我们现在不再使用软件模拟器了。

我们这时候已经成功的使用了rtc的周期时钟中断，Guest OS运行期间，你可以再SOS的/proc/interrupt下看到rtc产生的中断。

从性能角度上将，RTC时钟除了可以直接使用其中断外，还可以将控制端口注射到VMCS提供的io port map中，让其直接访问，而不会在访问的时候发生vm exit， 从而减少上下文切换，提高性能， 具体方法大家自己研究，我点到为止。

对于第二个缺陷，目前的xen虚拟机无法避免，因为它将传统的“中断发生－>中断处理”的同步方式改变成了异步方式。也就是中断发生后并非立刻被处理，而是要等到再次调度到目的guest os才能被处理，由于调度关系，中断处理时机存在很大的不确定行，因此不可避免的会出现中断不准确和丢失等问题。

如果想解决该问题，可能需要用到VT处理器手册（IA-32 Intel® Architecture Software Developer’s Manual Volume 3B: System Programming Guide, Part 2 、21-6页）中提到的特性―― **External interrupts.** An external interrupt causes a VM exit if the “external-interrupt exiting” VM-execution control is 1. Otherwise, **the interrupt is delivered normally through** **the IDT**。至于如何使用该特性就留给读者思考吧。

## 总结

虚拟机的性能目前是其应用推广的瓶颈，因此一切有关虚拟化的技术都不得不考虑性能（至于实时性的考虑目前来说还尚早）。从性能角度讲，大约可从下面几个方面入手—vm exit的次数，xen中断转发流程和效率，xen的调度，设备模拟效率等等。我这里以时钟设备为例子进行讨论，希望可帮助大家加深对xen的运行机制的了解以及对时钟的认识。

附录 RTC时钟硬件知识 (摘自第七章 Linux内核的时钟中断 (上) By 詹荣开，NUDT)

7.1 时钟硬件
7.1.1 实时时钟RTC
自从IBM PCAT起，所有的PC机就都包含了一个叫做实时时钟（RTC）的时钟芯片，以便在PC机断电后仍然能够继续保持时间。显然，RTC是通过主板上的电池来供电的，而不是通过PC机电源来供电的，因此当PC机关掉电源后，RTC仍然会继续工作。通常，CMOSRAM和RTC被集成到一块芯片上，因此RTC也称作“CMOSTimer”。最常见的RTC芯片是MC146818（Motorola）和DS12887（maxim），DS12887完全兼容于MC146818，并有一定的扩展。本节内容主要基于MC146818这一标准的RTC芯片。具体内容可以参考MC146818的Datasheet。
7.1.1.1 RTC寄存器
MC146818 RTC芯片一共有64个寄存器。它们的芯片内部地址编号为0x00～0x3F（不是I/O端口地址），这些寄存器一共可以分为三组：

* （1）时钟与日历寄存器组：共有10个（0x00~0x09），表示时间、日历的具体信息。在PC机中，这些寄存器中的值都是以BCD格式来存储的（比如23dec＝0x23BCD）。
* （2）状态和控制寄存器组：共有4个（0x0A~0x0D），控制RTC芯片的工作方式，并表示当前的状态。
* （3）CMOS配置数据：通用的CMOS RAM，它们与时间无关，因此我们不关心它。
  时钟与日历寄存器组的详细解释如下：
  * AddressFunction
  * 00Current second for RTC
  * 01Alarm second
  * 02Current minute
  * 03Alarm minute
  * 04Current hour
  * 05Alarm hour
  * 06Current day of week（01＝Sunday）
  * 07Current date of month
  * 08Current month
  * 09Current year（final two digits，eg：93）

状态寄存器A（地址0x0A）的格式如下：
其中：

* （1）bit［7］——UIP标志（Update in Progress），为1表示RTC正在更新日历寄存器组中的值，此时日历寄存器组是不可访问的（此时访问它们将得到一个无意义的渐变值）。
* （2）bit［6：4］——这三位是“除法器控制位”（divider-control bits），用来定义RTC的操作频率。各种可能的值如下：
  Divider bitsTime-base frequencyDivider ResetOperation Mode
  DV2DV1DV0
  0004.194304 MHZNOYES
  0011.048576 MHZNOYES
  01032.769 KHZNOYES
  110/1任何YESNO
  PC机通常将Divider bits设置成“010”。
* （3）bit［3：0］——速率选择位（Rate Selection bits），用于周期性或方波信号输出。
```
  RS bits4.194304或1.048578 MHZ32.768 KHZ
  RS3RS2RS1RS0周期性中断方波周期性中断方波
  0000NoneNoneNoneNone
  000130.517μs32.768 KHZ3.90625ms256 HZ
  001061.035μs16.384 KHZ
  0011122.070μs8.192KHZ
  0100244.141μs4.096KHZ
  0101488.281μs2.048KHZ
  0110976.562μs1.024KHZ
  01111.953125ms512HZ
  10003.90625ms256HZ
  10017.8125ms128HZ
  101015.625ms64HZ
  101131.25ms32HZ
  110062.5ms16HZ
  1101125ms8HZ
  1110250ms4HZ
  1111500ms2HZ
  PC机BIOS对其默认的设置值是“0110”。
```
状态寄存器B的格式如下所示：
各位的含义如下：
* （1）bit［7］——SET标志。为1表示RTC的所有更新过程都将终止，用户程序随后马上对日历寄存器组中的值进行初始化设置。为0表示将允许更新过程继续。
* （2）bit［6］——PIE标志，周期性中断使能标志。
* （3）bit［5］——AIE标志，告警中断使能标志。
* （4）bit［4］——UIE标志，更新结束中断使能标志。
* （5）bit［3］——SQWE标志，方波信号使能标志。
* （6）bit［2］——DM标志，用来控制日历寄存器组的数据模式，0＝BCD，1＝BINARY。BIOS总是将它设置为0。
* （7）bit［1］——24／12标志，用来控制hour寄存器，0表示12小时制，1表示24小时制。PC机BIOS总是将它设置为1。
* （8）bit［0］——DSE标志。BIOS总是将它设置为0。

状态寄存器C的格式如下：

* （1）bit［7］——IRQF标志，中断请求标志，当该位为1时，说明寄存器B中断请求发生。
* （2）bit［6］——PF标志，周期性中断标志，为1表示发生周期性中断请求。
* （3）bit［5］——AF标志，告警中断标志，为1表示发生告警中断请求。
* （4）bit［4］——UF标志，更新结束中断标志，为1表示发生更新结束中断请求。

状态寄存器D的格式如下：

* （1）bit［7］——VRT标志（Valid RAM and Time），为1表示OK，为0表示RTC已经掉电。
* （2）bit［6：0］——总是为0，未定义。

7.1.1.2 通过I/O端口访问RTC
在PC机中可以通过I/O端口0x70和0x71来读写RTC芯片中的寄存器。其中，端口0x70是RTC的寄存器地址索引端口，0x71是数据端口。
读RTC芯片寄存器的步骤是：

```
mov al, addr
out 70h, al ; Select reg_addr in RTC chip
jmp $+2 ; a slight delay to settle thing
in al, 71h ;
写RTC寄存器的步骤如下：
mov al, addr
out 70h, al ; Select reg_addr in RTC chip
jmp $+2 ; a slight delay to settle thing
mov al, value
out 71h, al
```