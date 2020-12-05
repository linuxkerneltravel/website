---
title: "Modification For Linux or vxworks’ Tlb Data Miss Exception Handle For E500 Core"
date: 2020-12-3T16:50:24+08:00
author: "康华"
keywords: ["map"]
categories : ["linux杂谈"]
banner : "img/blogimg/1.png"
summary : "When I developed some especial memory mapping routines in pm8560 board, I meet with a fascinating problem that took me at least two weeks to investigate and resolve. For avoiding similar issue in further block others, I describe it’s details below."
---
# Modification For Linux or vxworks’ Tlb Data Miss Exception Handle For E500 Core

​                                                Author Kang Hua

 

When I developed some especial memory mapping routines in pm8560 board, I meet with a fascinating problem that took me at least two weeks to investigate and resolve. For avoiding similar issue in further block others, I describe it’s details below.

 

## Background

In our system we need map PCI space to a given address before accessing it and unmap after accessing. It is a very high frequent operation at runtime, so map or unmap operation must be as fast as possible ( I estimate < 10um at least).

 

In legacy system which are belong to PowerPC family(such as that of 750/820) ,we implement map and unmap operations by validating or invalidating the valid field of corresponding segment register(of cause ,we must map that space with page table at system initialization on time), It is very fast for only few operations involved.

 

However, now our hardware is changed to pm8560. The architecture of pm8560 is e500 core, although it still is PowerPC processor, the MMU part of e500 is different from traditional PowerPC. It has no any DBAT register and segment register, and hardware do not implement page table directly any longer (The management of page table is up to system software now) but implement only TLB operations. So we can not using legacy way to map or unmap.

 

If we using page table to map or unmap huge space like PCI space at runtime, It proved that at least 7 ms for mapping or unmapping 16M spaces every time(because we need modify 4*1024 page descriptors for each operations), In a real time ,we can not bear so long latency time. So we must find anther way to implement map or unmap operations.

 

After studying e500 manual, I decide to apply “PID” to speed up mapping and unmap process. I choose PID1 register with As to create a certain block Virtual Addresses for accessing PCI space on the fly, of cause it must be after mapping pci space (this space is mmapped with page table at boot time once as legacy way ,and after that every time we can unmap or remmap it with switch PID1 to 0 or 3)----but for PCI space, I set TID of according TLB descriptor in page table to 3 .

 

It is the key to accelerate map operations——As accessing PCI space , we set PID1 =3 ,which work is to open (map) PCI space; As finishing access PCI space, we set PID1=0，which work is to close (unmap) PCI space. Because "e500 constructs three virtual addresses for each access , All of the current values in the PID registers are used in the TLB look-up process and compared with the TID field in all the TLBs, if any of the PID values in PID0-PID2 matches with a TLB entry in which all the other match criteria are met ,the entry is used for translation." (PowerPC e500 Core Family Reference Manual ,Rev,1 ,page 12-5).The above process is apparently faster than operations of so much page table entries.

 

## TLB DATA Miss Exception Defect Description

When switching on PID1(set PID=3), we can access the mapped space normally. However, when switching off PID1(set PID=0), system hang up.

 

I do some experiments and found at that time system actually go into an infinite loop to access the address that's according TID =3. After checking the tlb data miss exception handler of e500 source code, I find the loop reason is not check tlb descriptor's TID field and PID register, so when we access above address, a tlb data miss exception is reported, but the linux’s( in head_e500.S) or vxworks’s (in mmuE500ALib.sc) exception handler only get related tlb descriptor form page table by address information and then put it into tlb. Here is the rub. As we known, TLB Data Miss exception is a hardware fault(Fault means after resolve hardware fault, cpu will re-execute the instruction that cause fault again).So after exception handler put a new tlb descriptor into tlb. Hardware (CPU) assume the address now is ok, and re-access it again. however because related tlb entry’s tid=3 in TLB table, while none pid register is 3 now, so cpu find no tlb entry matching, and an tlb miss exception will follow up again. Consequently system will enter a loop for ever.

 

​       

## Resolution

The original execute step of TLB Data Miss Exception handler of e500 core is

\1.   Save execute context

\2.   Search page table for the tlb entry that match above address.

\3.   If find, Update TLB with found tlb entry and restore context&return

\4.   If find no, restore context and jump to “instruction access exception”

 

There is lack of checking PID registers and tid field of tlb descriptor in page table. So even if tid is equal to any PID regisgters, handler still load according tlb descriptor into TLB table. It is fault in our case, so I add some check code snap in exception handler, which will compare PID1/PID2 with tid field of tlb entries. only at least one of PID registers’ content is equal to tid field, handler update TLB with according tlb entrie.

 

The execute step change into:

 

\1.   Save execute context

\2.   Search page table for the tlb entry that match above address.

\3.   If find, then check whether PID registers’ content is equal to tid field of fond tlb entry.

\4.   If check pass , Update TLB with found tlb entry and restore context&return

\5.   If find no or check fail, restore context and jump to “instruction access exception”

 

​    The check code snap list below:

​      *rlwinm. r19, r20,0x10,24,31*   

​      *beq   normal*

​      *mfspr r23,PID1*              

​      *xor.  r24,r19,r23*       

​      *beq    normal*

​      *mfspr r23,PID2*

​      *xor.  r24,r19,r23*

​      *beq    normal*