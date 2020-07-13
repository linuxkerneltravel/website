---
title: "字符设备驱动分析(1)"
date: 2020-07-13T09:39:36+08:00
author: "薛晓雯编辑"
keywords: ["字符设备","驱动模型"]
categories : ["文件系统"]
banner : "img/blogimg/makefile1.png"
summary : "熟悉了模块编程的基本框架后，我们就可以试着分析一个简单的字符设备驱动。下面以《设备驱动开发详解》一书中的代码6.17为例来分析这个字符设备驱动的代码。"
---

熟悉了模块编程的基本框架后，我们就可以试着分析一个简单的字符设备驱动。下面以《设备驱动开发详解》一书中的代码6.17为例来分析这个字符设备驱动的代码。

我们现在对于对前文中hello，kernel内核模块进行稍微的改动。我们都知道内核模块的入口函数是module_init（function name）内注册的函数。也就是告诉内核“从这个函数入口”。那么我们分析字符设备驱动模块，首先应该去看globalmem_init函数。


```c
module_init(globalmem_init);
module_exit(globalmem_exit);
```

在globalmem_init函数中，首先通过宏MKDEV获得32位的设备驱动号。通常linux中的设备号由主设备号和次设备号组成，主设备号对应每一类设备，而次设备号对应该类设备的具体一个设备。dev_t类型前12位是主设备号（但事实上主设备号的前四位被屏蔽了，如果去看源码就可得知），后20位为次设备号。由于可以事先指定主设备号globalmem_major，因此我们需要用宏MKDEV来获得dev_t类型的设备号：

```c
#define MINORBITS       20
#define MKDEV(ma,mi)    (((ma) << MINORBITS) | (mi))
```

这个宏通过移位和或运算，巧妙的得到dev_t类型的设备号。这个宏可以在include/linux/kdev_t.h中查找到。网上有的资料中会给出MINORBITS为8，这个应该是适合16位的设备号的情况。

接着通过全局变量globamem_major来判断是否事先分配了起始的设备号。如果是，则继续分配连续的一段设备号，否则动态分配设备号，并且通过MAJOR宏获得分配以后的主设备号。这里需要强调的是，下面的两种设备号分配函数，都是一次性分配一组连续的设备号（当然也可以只分配一个设备号，调整参数即可）。

首先我们分析已知起始设备号的情况。通过调用register_chrdev_region函数，便可以申请到一组连续范围的设备号。在linux/fs/char_dev.c中可以看到此函数的原型。

```c
int register_chrdev_region(dev_t from, unsigned count, const char *name);
```


其中from是首设备号，而count是这组连续设备号的数目。name为设备名。而《设备》一书中的count为1，也就是说，这组设备号的数量为1。

其次，当起始设备号并未指定时就要动态的申请了，使用下面的函数：

```c
int alloc_chrdev_region(dev_t *dev, unsigned baseminor, unsigned count,const char *name);
```

与上面静态分配函数不同的是，此时dev是一个指针类型，因为要返回将要分配的设备号。关于以上两个函数的内核源码分析，具体参见这里。

以上都成功执行后，我们给globalmem_devp指针分配内存空间。其中globalmem_devp是一个指向globalmem设备结构体的指针。我们通过mmset对此块内存空间进行初始化之后。接着globalmem_setup_cdev函数对cdev初始化并将此字符设备注册到内核。


```c
//设备驱动模块加载函数
int globalmem_init(void)
{

    int result;
    dev_t devno=MKDEV(globalmem_major,0);

    if(globalmem_major)
    {
      result=register_chrdev_region(devno,1,"globalmem");
    }
    else
    {
        result=alloc_chrdev_region(&devno,0,1,"globalmem");
        globalmem_major=MAJOR(devno);
    }
    if(result<0)
        return result;
    globalmem_devp=kmalloc(sizeof(struct globalmem_dev),GFP_KERNEL);
    if(!globalmem_devp)
    {
        result=-ENOMEM;
        goto fail_malloc;
    }
    
    memset(globalmem_devp,0,sizeof(struct globalmem_dev));
    
    globalmem_setup_cdev(globalmem_devp,0);
    return 0;
fail_malloc:unregister_chrdev_region(devno,1);
    return result;

}
```

上面的程序已经很“整齐”的说明了init函数的主要作用。具体如下：

1.申请设备号

2.为设备相关的数据结构分配内存

3.初始化并注册cdev(globalmem_setup_cdev函数实现)

有了字符设备驱动的加载函数，那么肯定有卸载函数：

```c
void globalmem_exit(void)
{
cdev_del(&globalmem_devp->cdev);
kfree(globalmem_devp);
unregister_chrdev_region(MKDEV(globalmem_major, 0), 1);
}
```

总体来看，globalmem_init函数完成的是字符设备的一些初始化工作，以及向系统内注册。而globalmem_exit就是进行字符设备的释放工作：从内核中删除这个字符设备，释放设备结构体所占的内存，以及释放申请的设备号。从结构上看，它并没有偏离内核模块编程的结构范围，仍然是我们熟悉的hello,kernel。

我们在此先暂时将globalmem_setup_cdev函数内部的实现用return 0;来代替。那么我们现在用插入内核模块的命令将我们这个字符设备驱动（尽管许多功能还未实现）插入到内核中。成功后我们可以通过cat /proc/devices 命令来查看刚刚字符设备名称以及主设备号。/proc是一个虚拟的文件系统，这里的虚拟是指这个文件系统并不占用磁盘空间而只存在于内存中。而该目录下的devices文件中存储着系统字符和块设备的驱动名称以及设备编号。

接下来，我们可以通过:mknod /dev/globalmem c 250 0命令创建一个设备节点。有了这个设备节点后，就可以对它进行类似普通文件那样的操作了（当然现在还不能，因为并未实现具体的操作函数）。

这样一个字符驱动的大致上有了雏形。