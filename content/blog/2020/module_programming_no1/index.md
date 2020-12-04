---
title: "内核模块编程之进阶（五）-授人以渔"
date: 2008-11-09T16:50:24+08:00
author: "helight0"
keywords: ["内核模块"]
categories : ["走进内核"]
banner : "img/blogimg/2.png"
summary : "在上一部分“编写带有参数的中断模块”中，这个看似简单的程序，你调试并运行以后思考了哪些方面的问题？ "
---

给模块传递参数，使得这个模块的扩展和应用有了空间。例如，在我的机器上查看/proc/interrupts

```shell
# cat /proc/interrupts 
            CPU0       CPU1       CPU2       CPU3       
   0:          4          0          0          0   IO-APIC    2-edge      timer
   1:       1528          0          0       4671   IO-APIC    1-edge      i8042
   8:          1          0          0          0   IO-APIC    8-edge      rtc0
   9:          0          0          0          0   IO-APIC    9-fasteoi   acpi
  12:       1527          0     106440          0   IO-APIC   12-edge      i8042
  14:          0          0          0          0   IO-APIC   14-edge      ata_piix
  15:          0          0          0          0   IO-APIC   15-edge      ata_piix
  16:        254      71759          0          0   IO-APIC   16-fasteoi   vmwgfx, snd_ens1371
  17:      27453          0          0          0   IO-APIC   17-fasteoi   ehci_hcd:usb1, ioc0
  18:          0         62          0          0   IO-APIC   18-fasteoi   uhci_hcd:usb2
  19:          0         56          0      14293   IO-APIC   19-fasteoi   ens33
  24:          0          0          0          0   PCI-MSI 344064-edge      PCIe PME, pciehp
  25:          0          0          0          0   PCI-MSI 346112-edge      PCIe PME, pciehp
  26:          0          0          0          0   PCI-MSI 348160-edge      PCIe PME, pciehp
  27:          0          0          0          0   PCI-MSI 350208-edge      PCIe PME, pciehp
  28:          0          0          0          0   PCI-MSI 352256-edge      PCIe PME, pciehp
  29:          0          0          0          0   PCI-MSI 354304-edge      PCIe PME, pciehp
  30:          0          0          0          0   PCI-MSI 356352-edge      PCIe PME, pciehp
  31:          0          0          0          0   PCI-MSI 358400-edge      PCIe PME, pciehp
  32:          0          0          0          0   PCI-MSI 360448-edge      PCIe PME, pciehp
  33:          0          0          0          0   PCI-MSI 362496-edge      PCIe PME, pciehp
  34:          0          0          0          0   PCI-MSI 364544-edge      PCIe PME, pciehp
  35:          0          0          0          0   PCI-MSI 366592-edge      PCIe PME, pciehp
  36:          0          0          0          0   PCI-MSI 368640-edge      PCIe PME, pciehp
  37:          0          0          0          0   PCI-MSI 370688-edge      PCIe PME, pciehp
  38:          0          0          0          0   PCI-MSI 372736-edge      PCIe PME, pciehp
  39:          0          0          0          0   PCI-MSI 374784-edge      PCIe PME, pciehp
  40:          0          0          0          0   PCI-MSI 376832-edge      PCIe PME, pciehp
  41:          0          0          0          0   PCI-MSI 378880-edge      PCIe PME, pciehp
  42:          0          0          0          0   PCI-MSI 380928-edge      PCIe PME, pciehp
  43:          0          0          0          0   PCI-MSI 382976-edge      PCIe PME, pciehp
  44:          0          0          0          0   PCI-MSI 385024-edge      PCIe PME, pciehp
  45:          0          0          0          0   PCI-MSI 387072-edge      PCIe PME, pciehp
  46:          0          0          0          0   PCI-MSI 389120-edge      PCIe PME, pciehp
  47:          0          0          0          0   PCI-MSI 391168-edge      PCIe PME, pciehp
  48:          0          0          0          0   PCI-MSI 393216-edge      PCIe PME, pciehp
  49:          0          0          0          0   PCI-MSI 395264-edge      PCIe PME, pciehp
  50:          0          0          0          0   PCI-MSI 397312-edge      PCIe PME, pciehp
  51:          0          0          0          0   PCI-MSI 399360-edge      PCIe PME, pciehp
  52:          0          0          0          0   PCI-MSI 401408-edge      PCIe PME, pciehp
  53:          0          0          0          0   PCI-MSI 403456-edge      PCIe PME, pciehp
  54:          0          0          0          0   PCI-MSI 405504-edge      PCIe PME, pciehp
  55:          0          0          0          0   PCI-MSI 407552-edge      PCIe PME, pciehp
  56:          0        112      11460          0   PCI-MSI 1130496-edge      ahci[0000:02:05.0]
  57:          0          0          0          0   PCI-MSI 129024-edge      vmw_vmci
  58:          0          0          0          0   PCI-MSI 129025-edge      vmw_vmci
 NMI:          0          0          0          0   Non-maskable interrupts
 LOC:     976787     307883     248740     258994   Local timer interrupts
 SPU:          0          0          0          0   Spurious interrupts
 PMI:          0          0          0          0   Performance monitoring interrupts
 IWI:          0          0          0          2   IRQ work interrupts
 RTR:          0          0          0          0   APIC ICR read retries
 RES:     231148     202631     219048     215288   Rescheduling interrupts
 CAL:      15875      19615      20240      18922   Function call interrupts
 TLB:      32096      22394      25769      24188   TLB shootdowns
 TRM:          0          0          0          0   Thermal event interrupts
 THR:          0          0          0          0   Threshold APIC interrupts
 DFR:          0          0          0          0   Deferred Error APIC interrupts
 MCE:          0          0          0          0   Machine check exceptions
 MCP:         74         75         75         75   Machine check polls
 ERR:          0
 MIS:          0
 PIN:          0          0          0          0   Posted-interrupt notification event
 NPI:          0          0          0          0   Nested posted-interrupt event
 PIW:          0          0          0          0   Posted-interrupt wakeup event

```

- 在插入模块时，你对每个中断都作为参数试运行一下，看看会出现什么问题？
- 思考一下irq为0,3等值时，为什么插入失败？
- 这就引出中断的共享和非共享问题，从而促使你分析Linux对共享的中断到底如何处理，共享同一个中断号的中断处理程序到底如何执行？ 

对于myinterrupt（）函数，可以进行怎样的改进，使得这个自定义的中断处理程序变得有实际意义？

```C
 static irqreturn_t myinterrupt(int irq, void *dev_id, struct pt_regs *regs) {
     static int mycount = 0;
     if (mycount < 10) {
         printk("Interrupt!\n");
         mycount++; } 
     return IRQ_NONE; 
 } 
```

​	比如，对于网卡中断，在此收集每一次中断发生时，从网卡接收到的数据，把其存入到文件中。以此思路，随你考虑应用场景了。

- [x] 模块机制给Linux内核的扩展和应用提供了方便的入口，在我们内核之旅[http://http://wwww.kerneltravel.net/](http://wwww.kerneltravel.net/) 的电子杂志部分，针对内核相关的内容，每一部分都有相对比较实际的内核应用题目，感兴趣者可以去实践，前提是对内核相关内容的彻透理解。