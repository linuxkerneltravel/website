---
title: "Linux设备驱动模型（四）-核心对象之演绎"
date: 2020-07-27T18:22:39+08:00
author: "编辑：戴君毅"
keywords: ["设备模型"]
categories : ["文件系统"]
banner : "img/blogimg/filesystem.jpeg"
summary : "话说kboject是驱动模型的核心对象，但在sysfs文件系统中似乎并没有对应的项，而这种看似“无”，实际上蕴藏着“有”。"
---

话说kboject是驱动模型的核心对象，但在sysfs文件系统中似乎并没有对应的项，而这种看似“无”，实际上蕴藏着“有”。

这“有”从何说起。回想文件系统中的核心对象“索引节点（indoe）”和目录项“dentry”：

inode—与文件系统中的一个文件相对应（而实际上，只有文件被访问时，才在内存创建索引节点）。

dentry—每个路径中的一个分量，例如路径/bin/ls，其中/、bin和ls三个都是目录项，只是前两个是目录，而最后一个是普通文件。也就是说，目录项或者是一子目录，或者是一个文件。

从上面的定义可以看出，indoe和dentry谁的包容性更大？当然是dentry！那么，kobject与dentry有何关系？由此我们可以推想，把dentry作为kobject中的一个字段，恍然间，kobject变得强大起来了。何谓“强大”，因为这样一来，就可以方便地将kobject映射到一个dentry上，也就是说，kobject与/sys下的任何一个目录或文件相对应了，进一步说，把kobject导出形成文件系统就变得如同在内存中构建目录项一样简单。由此可知，kobject其实已经形成一棵树了。这就是以隐藏在背后的对象模型为桥梁，将驱动模型和sysfs文件系统全然联系起来。由于kobject被映射到目录项，同时对象模型层次结构也已经在内存形成一个树，因此sysfs的形成就水到渠成了。

既然，kobject要形成一颗树，那么其中的字段就要有parent，以表示树的层次关系；另外，kobject不能是无名氏，得有name字段，按说，目录或文件名并不会很长，但是，sysfs文件系统为了表示对象之间复杂的关系，需要通过软链接达到，而软链接常常有较长的名字，通过以上的分析，目前可以得知kobject对象包含的字段有：

```
struct kobject {

    char       *k_name;  /*长名字*/
    
    char       name[kOBJ_NAME_LEN]; /* 短名字*/
    
    struct kobject    *parent; /* 表示对象的层次关系*/
    
    struct dentry *dentry; /*表示sysfs中的一个目录项 */

};
```

分析到这里，似乎已经知道kobject说包含的字段了，但且慢，查看kobject.h头文件，看到它还包含以下字段：

```
struct kobject {

    struct kref          kref;
    
    struct list_head  entry;
    
    struct kset          *kset;
    
    struct kobj_type  *ktype;

};
```

这四个字段，每一个都是结构体，其中structlist_head是内核中形成双向链表的基本结点结构，而其他三个结构体存在的理由是什么？

（1）引用计数kref

kobject的主要功能之一就是为我们提供了一个统一的引用计数系统，为什么说它具有“统一”的能力？那是因为kobject是“基”对象，就像大厦的基地，其他对象（如devic,bus,class,device_driver等容器）都将其包含，以后，其他对象的引用技术继承或封装kobject的引用技术就可以了。

初始化时，kobject的引用计数设置为1。只要引用计数不为零，那么该对象就会继续保留在内存中，也可以说是被“钉住”了。任何包含对象引用的代码首先要增加该对象的引用计数，当代码结束后则减少它的引用计数。增加引用计数称为获得（getting）对象的引用，减少引用计数称为释放(putting)对象的引用。当引用计数跌到零时，对象便可以被销毁，同时相关内存也都被释放。COM中的IUnKnown亦实现了AddRef()/Release()引用计数接口。

增加一个引用计数可通过koject_get()函数完成：

struct kobject* kobject_get(struct kobject *kobj);

该函数正常情况下将返回一个指向kobject的指针，如果失败则返回NULL指针；

减少引用计数通过kobject_put()完成：

void kobject_put(struct kobject *kobj);

如果对应的kobject的引用计数减少到零，则与该kobject关联的ktype中的析构函数将被调用。

我们深入到引用计数系统的内部去看，会发现kobject的引用计数是通过kref结构体实现的，该结构体定义在头文件<linux/kref.h>中：

struct kref {atomic_t refcount;};

其中唯一的字段是用来存放引用计数的原子变量。那为什么采用结构体？这是为了便于进行类型检测。在使用kref前，必须先通过kref_init()函数来初始化它：

void kref_init(struct kref *kref) { atomic_set(&kref->refcount,1); }

正如你所看到的，这个函数简单的将原子变量置1，所以kref一但被初始化，它表示的引用计数便固定为1。

开发者现在不必在内核代码中利用atmoic_t类型来实现其自己的引用计数。对开发者而言，在内核代码中最好的方法是利用kref类型和它相应的辅助函数，为自己提供一个通用的、正确的引用计数机制。

上述的所有函数定义与声明分别在在文件lib/kref.c和文件<linux/kref.h>中。

（2）共同特性的ktype

如上所述，kobject是一个抽象而基本的对象。对于一族具有共同特性的kobject，就是用定义在头文件<linux/kobject.h>中的ktype来描述：

```
struct kobj_type {     

    void (*release)(structkobject *);
    
    struct sysfs_ops  *sysfs_ops;
    
    struct attribute  **default_attrs;

};
```

release指针指向在kobject引用计数减至零时要被调用的析构函数。该函数负责释放所有kobject使用的内存和其它相关清理工作。

sysfs_ops变量指向sysfs_ops结构体，其中包含两个函数，也就是对属性进行操作的读写函数show()和store()。

最后，default_attrs指向一个attribute结构体数组。这些结构体定义了kobject相关的默认属性。属性描述了给定对象的特征，其实，属性就是对应/sys树形结构中的叶子结点，也就是文件。

（3）对象集合体kset

kset，顾名思义就是kobject对象的集合体，可以把它看成是一个容器，可将所有相关的kobject对象聚集起来，比如“全部的块设备”就是一个kset。听起来kset与ktypes非常类似，好像没有多少实质内容。那么“为什么会需要这两个类似的东西呢”。ksets可把kobject集中到一个集合中，而ktype描述相关类型kobject所共有的特性，它们之间的重要区别在于：具有相同ktype的kobject可以被分组到不同的ksets。

kobject的kset指针指向相应的kset集合。kset集合由kset结构体表示，定义于头文件<linux/kobject.h>中：

```
struct kset {

    struct kobj_type     *ktype;
    
    struct list_head     list;
    
    struct kobject           kobj;
    
    struct kset_uevent_ops   *uevent_ops;

};
```

其中ktype指针指向集合（kset）中kobject对象的类型（ktype），list连接该集合（kset）中所有的kobject对象。kobj指向的koject对象代表了该集合的基类，uevent_ops指向一个用于处理集合中kobject对象的热插拔操作的结构体。

总结：kobject通常是嵌入到其它结构中的，其单独意义其实并不大。相反，那些更为重要的结构体，比如在struct cdev中才真正需要用到kobject结构。

```
/* cdev structure – 该对象代表一个字符设备 */

struct cdev {

    struct kobject           kobj;
    
    struct module        *owner;
    
    struct file_operations   *ops;
    
    struct list_head     list;
    
    dev_t                dev;
    
    unsigned int         count;

};
```

当kobject被嵌入到其它结构中时，该结构便拥有了kobject提供的标准功能。更重要的一点是，嵌入kobject的结构体可以成为对象层次架构中的一部分。比如cdev结构体就可通过其父指针cdev->kobj->parent和链表cdev->kobj->entry来插入到对象层次结构中。