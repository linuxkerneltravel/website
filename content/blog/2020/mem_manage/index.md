---
title: "虚拟内存管理实例"
date: 2020-11-23T10:42:47+08:00
author: "梁鹏整理"
keywords: ["内存管理"]
categories : ["走进内核"]
banner : "img/blogimg/linux_wechat.png"
summary : "本文是关于虚拟内存管理的一个实例，通过我们写入的字符来调用不同的处理函数，打印不同的信息"
---

关于虚拟内存管理的基本内容参看相关内容，在此不再赘述，在理论的基础上，设计出合理的实验可以强化对理论的理解和应用能力 。

**实验内容**：在proc 文件系统下，建立一个文件，每次向这个文件写人字符时，调用相应的虚拟内存处理函数

```c
/*
mtest_dump_vma_list（）：打印出当前进程的各个VMA，这个功能我们简称"listvma"
mtest_find_vma()： 找出某个虚地址所在的VMA，这个功能我们简称“findvma"
my_follow_page( )：根据页表，求出某个虚地址所在的物理页面，这个功能我们简称"findpage"
mtest_write_val(), 在某个地址写上具体数据，这个功能我们简称“writeval".
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/proc_fs.h>
#include <linux/string.h>
#include <linux/vmalloc.h>
#include <asm/uaccess.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
MODULE_LICENSE("GPL");
/*
@如何编写代码查看自己的进程到底有哪些虚拟区？
*/
static void mtest_dump_vma_list(void)
{
struct mm_struct *mm = current->mm;
struct vm_area_struct *vma;
printk("The current process is %s\n",current->comm);
printk("mtest_dump_vma_list\n");
down_read(&mm->mmap_sem);
for (vma = mm->mmap;vma; vma = vma->vm_next) {
printk("VMA 0x%lx-0x%lx ",
vma->vm_start, vma->vm_end);
if (vma->vm_flags & VM_WRITE)
printk("WRITE ");
if (vma->vm_flags & VM_READ)
printk("READ ");
if (vma->vm_flags & VM_EXEC)
printk("EXEC ");
printk("\n");
}
up_read(&mm->mmap_sem);
}
/*
@如果知道某个虚地址，比如，0x8049000,
又如何找到这个地址所在VMA是哪个？
*/
static void  mtest_find_vma(unsigned long addr)
{
struct vm_area_struct *vma;
struct mm_struct *mm = current->mm;
printk("mtest_find_vma\n");
down_read(&mm->mmap_sem);
vma = find_vma(mm, addr);
if (vma && addr >= vma->vm_start) {
printk("found vma 0x%lx-0x%lx flag %lx for addr 0x%lx\n",
vma->vm_start, vma->vm_end, vma->vm_flags, addr);
} else {
printk("no vma found for %lx\n", addr);
}
up_read(&mm->mmap_sem);
}
/*
@一个物理页在内核中用struct page来描述。
给定一个虚存区VMA和一个虚地址addr，
找出这个地址所在的物理页面page.
*/
static struct page *
my_follow_page(struct vm_area_struct *vma, unsigned long addr)
{
pud_t *pud;
pmd_t *pmd;
pgd_t *pgd;
pte_t *pte;
spinlock_t *ptl;
struct page *page = NULL;
struct mm_struct *mm = vma->vm_mm;
pgd = pgd_offset(mm, addr);
if (pgd_none(*pgd) || unlikely(pgd_bad(*pgd))) {
goto out;
}
pud = pud_offset(pgd, addr);
if (pud_none(*pud) || unlikely(pud_bad(*pud)))
goto out;
pmd = pmd_offset(pud, addr);
if (pmd_none(*pmd) || unlikely(pmd_bad(*pmd))) {
goto out;
}
pte = pte_offset_map_lock(mm, pmd, addr, &ptl);
if (!pte)
goto out;
if (!pte_present(*pte))
goto unlock;
page = pfn_to_page(pte_pfn(*pte));
if (!page)
goto unlock;
get_page(page);
unlock:
pte_unmap_unlock(pte, ptl);
out:
return page;
}
/*
@ 根据页表，求出某个虚地址所在的物理页面，
这个功能我们简称"findpage"
*/
static void   mtest_find_page(unsigned long addr)
{
struct vm_area_struct *vma;
struct mm_struct *mm = current->mm;
unsigned long kernel_addr;
struct page *page;
printk("mtest_write_val\n");
down_read(&mm->mmap_sem);
vma = find_vma(mm, addr);
page = my_follow_page(vma, addr);
if (!page)
{
printk("page not found  for 0x%lx\n", addr);
goto out;
}
printk("page  found  for 0x%lx\n", addr);
kernel_addr = (unsigned long)page_address(page);
kernel_addr += (addr&~PAGE_MASK);
printk("find  0x%lx to kernel address 0x%lx\n", addr, kernel_addr);
out:
up_read(&mm->mmap_sem);
}
/*
@你是否有这样的想法，
给某个地址写入自己所想写的数据？
*/
static void
mtest_write_val(unsigned long addr, unsigned long val)
{
struct vm_area_struct *vma;
struct mm_struct *mm = current->mm;
struct page *page;
unsigned long kernel_addr;
printk("mtest_write_val\n");
down_read(&mm->mmap_sem);
vma = find_vma(mm, addr);
if (vma && addr >= vma->vm_start && (addr + sizeof(val)) < vma->vm_end) {
if (!(vma->vm_flags & VM_WRITE)) {
printk("vma is not writable for 0x%lx\n", addr);
goto out;
}
page = my_follow_page(vma, addr);
if (!page) {
printk("page not found  for 0x%lx\n", addr);
goto out;
}
kernel_addr = (unsigned long)page_address(page);
kernel_addr += (addr&~PAGE_MASK);
printk("write 0x%lx to address 0x%lx\n", val, kernel_addr);
*(unsigned long *)kernel_addr = val;
put_page(page);
} else {
printk("no vma found for %lx\n", addr);
}
out:
up_read(&mm->mmap_sem);
}
static ssize_t
mtest_write(struct file *file, const char __user * buffer,
size_t count, loff_t * data)
{
printk("mtest_write  ...........  \n");
char buf[128];
unsigned long val, val2;
if (count > sizeof(buf))
return -EINVAL;
if (copy_from_user(buf, buffer, count))
return -EINVAL;
if (memcmp(buf, "listvma", 7) == 0)
mtest_dump_vma_list();
else if (memcmp(buf, "findvma", 7) == 0) {
if (sscanf(buf + 7, "%lx", &val) == 1) {
mtest_find_vma(val);
}
}
else if (memcmp(buf, "findpage", 8) == 0) {
if (sscanf(buf + 8, "%lx", &val) == 1) {
mtest_find_page(val);
//my_follow_page(vma, addr);
}
}
else  if (memcmp(buf, "writeval", 8) == 0) {
if (sscanf(buf + 8, "%lx %lx", &val, &val2) == 2) {
mtest_write_val(val, val2);
}
}
return count;
}
static struct
file_operations proc_mtest_operations = {
.write        = mtest_write
};
static struct proc_dir_entry *mtest_proc_entry;
//整个操作我们以模块的形式实现，因此，模块的初始化和退出函数如下：
static int __init
mtest_init(void)
{
mtest_proc_entry = create_proc_entry("mtest", 0777, NULL);
if (mtest_proc_entry == NULL) {
printk("Error creating proc entry\n");
return -1;
}
printk("create the filename mtest mtest_init sucess  \n");
mtest_proc_entry->proc_fops = &proc_mtest_operations;
return 0;
}
static void
__exit mtest_exit(void)
{
printk("exit the module......mtest_exit \n");
remove_proc_entry("mtest", NULL);
}
MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("mtest");
MODULE_AUTHOR("Zou Nan hai");
module_init(mtest_init);
module_exit(mtest_exit);
```

下面为Makefile

```c
obj-m := mm.o
# KDIR is the location of the kernel source.  The current standard is
# to link to the associated source tree from the directory containing
# the compiled modules.
KDIR  := /lib/modules/$(shell uname -r)/build
# PWD is the current working directory and the location of our module
# source files.
PWD   := $(shell pwd)
# default is the default make target.  The rule here says to run make
# with a working directory of the directory containing the kernel
# source and compile only the modules in the PWD (local) directory.
default:
$(MAKE) -C $(KDIR) M=$(PWD) modules
clean:
rm -rf *.o *.ko *.mod.c
```

下面为测试用例

```c
[root@HBIDS proc]# echo "listvma" > mtest 
[root@HBIDS proc]# echo "listvma" > mtest 
[root@HBIDS proc]# echo "findvma0xb7f2b001" > mtest 
[root@HBIDS proc]# echo "findpage0xb7f2b001" > mtest 
[root@HBIDS proc]# echo "writeval0xb7f2b001 123456" > mtest 
打印结果为 
The current process is bash 
mtest_dump_vma_list 
VMA 0x8048000-0x80dc000 READ EXEC VMA 0x80dc000-0x80e2000 WRITE READ EXEC VMA 0x80e2000-0x811e000 WRITE READ EXEC VMA 0x42000000-0x4212e000 READ EXEC VMA 0x4212e000-0x42131000 WRITE READ EXEC VMA 0x42131000-0x42133000 WRITE READ EXEC VMA 0xb7d00000-0xb7f00000 READ EXEC VMA 0xb7f00000-0xb7f0b000 READ EXEC VMA 0xb7f0b000-0xb7f0c000 WRITE READ EXEC VMA 0xb7f0c000-0xb7f0d000 WRITE READ EXEC VMA 0xb7f0d000-0xb7f0f000 READ EXEC VMA 0xb7f0f000-0xb7f10000 WRITE READ EXEC VMA 0xb7f10000-0xb7f13000 READ EXEC VMA 0xb7f13000-0xb7f14000 WRITE READ EXEC VMA 0xb7f2b000-0xb7f31000 READ EXEC VMA 0xb7f31000-0xb7f32000 WRITE READ EXEC VMA 0xb7f32000-0xb7f47000 READ EXEC VMA 0xb7f47000-0xb7f48000 WRITE READ EXEC VMA 0xbfd31000-0xbfd47000 WRITE READ EXEC mtest_write  ........... mtest_find_vma found vma 0xb7f47000-0xb7f48000 flag 100877 for addr 0xb7f47001 mtest_write  ........... mtest_write_val page  found  for 0xb7f47001 find  0xb7f47001 to kernel address 0xc8c4e001 mtest_write  ........... mtest_write_val write 0x1234 to address 0xc8c4e001 
```

后记：这个程序是去年八月份发表在我的博客上的，当时分为三篇发表。也许因为新博客搬家的缘故找不到了。最近的教学中需要这个例子，于是在网上去搜，幸好有网友收藏并进行了整理，在此贴出，以让大家对抽象的虚存管理有代码级的理解。 