---
title: "字符设备驱动分析(2)"
date: 2020-07-19T20:08:48+08:00
author: "薛晓雯编辑"
keywords: ["字符设备","驱动模型"]
categories : ["文件系统"]
banner : "img/blogimg/makefile1.png"
summary : "如何找到一个有效的切入点去深入分析内核源码，这是一个令人深思的问题。本文以前文中未详细说明的函数为切入点，深入分析char_dev.c文件的代码。如果你已经拥有了C语言基础和一些数据结构基础，那么还等什么？Let’s go！"
---

如何找到一个有效的切入点去深入分析内核源码，这是一个令人深思的问题。本文以前文中未详细说明的函数为切入点，深入分析char_dev.c文件的代码。如果你已经拥有了C语言基础和一些数据结构基础，那么还等什么？Let’s go！

在《字符设备驱动分析》一文中，我们说到register_chrdev_region函数的功能是在已知起始设备号的情况下去申请一组连续的设备号。不过大部分驱动书籍都没有去深入说明此函数，可能是因为这个函数内部封装了__register_chrdev_region(unsigned int major, unsigned int baseminor, int minorct, const char *name)函数的原因。不过我们不用苦恼，这正好促使我们去分析这个函数。

```c
int register_chrdev_region(dev_t from, unsigned count, const char *name)
{       struct char_device_struct *cd;
        dev_t to = from + count;
        dev_t n, next;

        for (n = from; n <\ to; n = next) {

               next = MKDEV(MAJOR(n)+1, 0);
               if (next >\ to)
                      next = to;
               cd = __register_chrdev_region(MAJOR(n), MINOR(n),
                              next - n, name);
               if (IS_ERR(cd))
                       goto fail;
      }
       return 0;
fail:
       to = n;
       for (n = from; n <\ to; n = next) {
               next = MKDEV(MAJOR(n)+1, 0);
               kfree(__unregister_chrdev_region(MAJOR(n), MINOR(n), next - n));
       }
       return PTR_ERR(cd);
}
```

首先值得我们注意的是，这个函数每次分配的是一组设备编号。其中from参数是这组连续设备号的起始设备号，count是这组设备号的大小（也是次设备号的个数），name参数处理本组设备的驱动名称。另外，当次设备号数目过多（count过多）的时候，次设备号可能会溢出到下一个主设备。因此我们在for语句中可以看到，首先得到下一个主设备号（其实也是一个设备号，只不过此时的次设备号为0）并存储于next中。然后判断在from的基础上再追加count个设备是否已经溢出到下一个主设备号。如果没有溢出（next小于to），那么整个for语句就只执行个一次__register_chrdev_region函数；否则当设备号溢出时，会把当前溢出的设备号范围划分为几个小范围，分别调用__register_chrdev_region函数。

如果在某个小范围调用__register_chrdev_region时出现了失败，那么会将此前分配的设备号都释放。

其实register_chrdev_region函数还没有完全说清除设备号分配的具体过程，因为具体某个小范围的设备号是由__register_chrdev_region函数来完成的。可能你已经注意到在register_chrdev_region函数源码中出现了struct char_device_struct结构，我们首先来看这个结构体：

```c
static struct char_device_struct {
        struct char_device_struct *next;
        unsigned int major;
        unsigned int baseminor;
        int minorct;
        char name[64];
        struct cdev *cdev;              /* will die */
} *chrdevs[CHRDEV_MAJOR_HASH_SIZE];
```

在register_chrdev_region函数中，在每个字符设备号的小范围上调用__register_chrdev_region函数，都会返回一个struct char_device_struct类型的指针。因此我们可以得知，struct char_device_struct类型对应的并不是每一个字符设备，而是具有连续设备号的一组字符设备。从这个结构体内部的字段也可以看出，这组连续的设备号的主设备号为major，次设备号起始为baseminor，次设备号范围为minorct，这组设备号对应的设备驱动名称为name，cdev为指向这个字符设备驱动的指针。

这里要特别说明的是，内核中所有已分配的字符设备编号都记录在一个名为chrdevs散列表里。该散列表中的每一个元素是一个 char_device_struct结构，这个散列表的大小为255（CHRDEV_MAJOR_HASH_SIZE），这是因为系统屏蔽了12位主设备号的前四位。既然说到散列表，那么肯定会出现冲突现象，因此next字段就是冲突链表中的下一个元素的指针。

接下来我们详细来析__register_chrdev_region函数。首先为cd变量分配内存并用零来填充（这就是用kzalloc而不是kmalloc的原因）。接着通过P操作使得后续要执行的语句均处于临界区。


```c
static struct char_device_struct *
__register_chrdev_region(unsigned int major, unsigned int baseminor,
                           int minorct, const char *name)
{
        struct char_device_struct *cd, **cp;
        int ret = 0;
        int i;

        cd = kzalloc(sizeof(struct char_device_struct), GFP_KERNEL);

        if (cd == NULL)
                return ERR_PTR(-ENOMEM);
        mutex_lock(&chrdevs_lock);
```

如果major为0，也就是未指定一个具体的主设备号，需要动态分配。那么接下来的if语句就在整个散列表中为这组设备寻找合适的位置，即从散列表的末尾开始寻找chrdevs[i]为空的情况。若找到后，那么i不仅代表这组设备的主设备号，也代表其在散列表中的关键字。当然，如果主设备号实现已指定，那么可不去理会这部分代码。

```c
/* temporary */
        if (major == 0) {
                for (i = ARRAY_SIZE(chrdevs)-1; i > 0; i--) {
                        if (chrdevs[i] == NULL)
                                break;
                }

                if (i == 0) {
                        ret = -EBUSY;
                        goto out;
                }
                major = i;
                ret = major;
        }
```

接着对将参数中的值依次赋给cd变量的对应字段。当主设备号非零，即事先已知的话，那么还要通过major_to_index函数对其进行除模255运算，因此整个散列表关键字的范围是0～254。

```c
        cd->major = major;
        cd->baseminor = baseminor;
        cd->minorct = minorct;
        strlcpy(cd->name, name, sizeof(cd->name));

        i = major_to_index(major);
```

至此，我们通过上面的代码会得到一个有效的主设备号（如果可以继续执行下面代码的话），那么接下来还不能继续分配。正如你所知的那样，散列表中的冲突是在所难免的。因此我们得到major的值后，我们要去遍历冲突链表，为当前我们所述的char_device_struct类型的变量cd去寻找正确的位置。更重要的是，我们要检查当前的次设备号范围，即baseminor~baseminor+minorct，是否和之前的已分配的次设备号（前提是major相同）范围有重叠。

下面的for循环就是在冲突链表中查找何时的位置，当出现以下三种情况时，for语句会停止。

(1)如果冲突表中正被遍历的结点的主设备号（*(cp)->major）大于我们所分配的主设备号(major)，那么就可以跳出for语句，不再继续查找。此时应该说设备号分配成功了，那么cd结点只需等待被插到冲突链表当中（*cp节点之前）。

(2)如果(*cp)结点和cd结点的主设备号相同，但是前者的次设备号起点比cd结点的大，那么跳出for语句，等待下一步的范围重叠的检测。

(3)如果(*cp)结点和cd结点的主设备号相同，但是cd结点的次设备号起点小于(*cp)结点的次设备号的终点，那么会跳出for语句。此时很可能两个范围的次设备号发生了重叠。

由上面的分析可以看出，冲突表中是按照设备号递增的顺序排列的。

```c
for (cp = &chrdevs[i]; *cp; cp = &(*cp)->next)

                if ((*cp)->major > major ||
                    ((*cp)->major ==major &&
                     (((*cp)->baseminor >= baseminor) ||
                      ((*cp)->baseminor + (*cp)->minorct > baseminor))))
                        break;
```

接下来检测当主设备号相同时，次设备范围是否发生了重叠。首先依次计算出新老次设备号的范围，接着进行范围判断。第一个判断语句是检测新范围的终点是否在老范围的之间；第二个判断语句是检测新范围的起点是否在老范围之间。

```c
/* Check for overlapping minor ranges.  */
        if (*cp && (*cp)->major == major) {
                int old_min = (*cp)->baseminor;
                int old_max = (*cp)->baseminor + (*cp)->minorct - 1;
                int new_min = baseminor;
                int new_max = baseminor + minorct - 1;

                /* New driver overlaps from the left.  */
                if (new_max >= old_min && new_max <= old_max) {
                        ret = -EBUSY;
                        goto out;
                }

                /* New driver overlaps from the right.  */
                if (new_min <= old_max && new_min >= old_min) {
                        ret = -EBUSY;
                        goto out;
                }
        }
```

当一切都正常后，就将char_device_struct描述符插入到中途链表中。至此，一次小范围的设备号分配成功。并且此时离开临界区，进行V操作。如果上述过程中有任何失败，则会跳转到out处，返回错误信息。

```c
cd->next = *cp;
        *cp = cd;
        mutex_unlock(&chrdevs_lock);
        return cd;
out:
        mutex_unlock(&chrdevs_lock);
        kfree(cd);
        return ERR_PTR(ret);
}
```

至此，我们已经分析完了字符设备号分配函数。