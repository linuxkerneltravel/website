---
title: "Linux内核中的红黑树"
date: 2020-07-12T14:17:52+08:00
author: "作者：王聪 编辑：张孝家"
keywords: ["红黑树"]
categories : ["经验交流"]
banner : "img/blogimg/zxj0.jpg"
summary : "这篇文章主要是对红黑树的工作原理做介绍"
---

红黑树是平衡二叉树的一种，它有很好的性质，树中的结点都是有序的，而且因为它本身就是平衡的，所以查找也不会出现非常恶劣的情况，基于二叉树的操作的时间复杂度是O(log(N))。Linux内核在管理vm_area_struct时就是采用了红黑树来维护内存块的。


先到include/linux/rbtree.h中看一下红黑树的一些定义，如下：

```
struct rb_node
{
unsigned long rb_parent_color;
#define RB_RED 0
#define RB_BLACK 1
struct rb_node *rb_right;
struct rb_node *rb_left;
} __attribute__((aligned(sizeof(long))));
```

struct rb_root只是struct rb_node`*`的一个包装，这样做的好处是看起来不用传递二级指针了。不错，很简单。再看一下下面几个重要的宏，细心的你一定会发现，rb_parent_color其实没那么简单，Andrea Arcangeli在这里使用了一个小的技巧，不过非常棒。正如名字所暗示，这个成员其实包含指向parent的指针和此结点的颜色！它是怎么做到的呢？很简单，对齐起了作用。既然是sizeof(long)大小的对齐，那么在IA-32上，任何rb_node结构体的地址的低两位肯定都是零，与其空着不用，还不如用它们表示颜色，反正颜色就两种，其实一位就已经够了。

这样，提取parent指针只要把rb_parent_color成员的低两位清零即可：
```
#define rb_parent(r) ((struct rb_node *)((r)->rb_parent_color & ~3))
```

取颜色只要看最后一位即可：

```
#define rb_color(r) ((r)->rb_parent_color & 1)
```


测试颜色和设置颜色也是水到渠成的事了。需要特别指出的是下面的一个内联函数：

```
static inline void rb_link_node(struct rb_node * node, struct rb_node * parent, struct rb_node ** rb_link);
```

它把parent设为node的父结点，并且让rb_link指向node。

我们把重点集中在lib/rbtree.c上，看看一些和红黑树相关的重要算法。开始之前我们一起回忆一下红黑树的规则：

1. 每个结点要么是红色要么是黑色；

2. 根结点必须是黑色；

3. 红结点如果有孩子，其孩子必须都是黑色；

4. 从根结点到叶子的每条路径必须包含相同数目的黑结点。

这四条规则可以限制一棵排序树是平衡的。


`__rb_rotate_left`是把以root为根的树中的node结点进行左旋，`__rb_rotate_right`是进行右旋。这两个函数是为后面的插入和删除服务，而不是为外部提供接口。


新插入的结点都设为叶子，染成红色，插入后如果破坏了上述规则，通过调整颜色和旋转可以恢复，二叉树又重新平衡。插入操作的接口函数是

```
void rb_insert_color(struct rb_node *node, struct rb_root *root);
```

它把已确定父结点的node结点融入到以root为根的红黑树中，具体算法的分析可以参考**[1]**中第14.3节，这里的实现和书中的讲解几乎完全一样。怎么确定node的父结点应该在调用rb_insert_color之前通过手工迭带完成。值得指出的一点是，虽然插入操作需要一个循环迭代，但是总的旋转次数不会超过两次！所以效率还是很乐观的。

删除操作多多少少都有点麻烦，它要先执行像普通二叉查找树的“删除”，然后根据删除结点的颜色来判断是否执行进一步的操作。删除的接口是：

```
void rb_erase(struct rb_node *node, struct rb_root *root);
```

其实它并没有真正删除node，而只是让它和以root为根的树脱离关系，最后它还要判断是否调用`__rb_erase_color`来调整。具体算法的讲解看参考**[1]**中第13.3和14.4节，`__rb_erase_colo`r对应书中的RB-DELETE-FIXUP，此处的实现和书上也基本上一致。

其余的几个接口就比较简单了。

```
struct rb_node *rb_first(struct rb_root *root);
```

在以root为根的树中找出并返回最小的那个结点，只要从根结点一直向左走就是了。

```
struct rb_node *rb_last(struct rb_root *root);
```

是找出并返回最大的那个，一直向右走。

```
struct rb_node *rb_next(struct rb_node *node);
```

返回node在树中的后继，这个稍微复杂一点。如果node的右孩子不为空，它只要返回node的右子树中最小的结点即可；如果为空，它要向上查找，找到迭带结点是其父亲的左孩子的结点，返回父结点。如果一直上述到了根结点，返回NULL。

```
struct rb_node *rb_prev(struct rb_node *node);
```

返回node的前驱，和rb_next中的操作对称。

```
void rb_replace_node(struct rb_node *victim, struct rb_node *new, struct rb_root *root);
```

用new替换以root为根的树中的victim结点。

红黑树接口使用的一个典型例子如下：

```
static inline struct page * rb_search_page_cache(struct inode * inode,
unsigned long offset)
{
struct rb_node * n = inode->i_rb_page_cache.rb_node;
struct page * page;

while (n)
{
page = rb_entry(n, struct page, rb_page_cache);

if (offset < page->offset)
n = n->rb_left;
else if (offset > page->offset)
n = n->rb_right;
else
return page;
}

return NULL;
}

static inline struct page * __rb_insert_page_cache(struct inode * inode,
unsigned long offset,
struct rb_node * node)
{
struct rb_node ** p = &inode->i_rb_page_cache.rb_node;
struct rb_node * parent = NULL;
struct page * page;

while (*p)
{
parent = *p;
page = rb_entry(parent, struct page, rb_page_cache);

if (offset < page->offset)
p = &(*p)->rb_left;
else if (offset > page->offset)
p = &(*p)->rb_right;
else
return page;
}

rb_link_node(node, parent, p);
return NULL;
}

static inline struct page * rb_insert_page_cache(struct inode * inode,
unsigned long offset,
struct rb_node * node)
{
struct page * ret;

if ((ret = __rb_insert_page_cache(inode, offset, node)))
goto out;
rb_insert_color(node, &inode->i_rb_page_cache);
out:
return ret;
}
```

因为红黑树的这些良好性质和实现中接口的简易性，它被广泛应用到内核编程中，大大提高了内核的效率。

**参考资料：**

1. *Introduction to Algorithms*, Thomas H. Cormen, Charles E. Leiserson, and Ronald L. Rivest, MIT Press.
2. *Understanding the Linux Kernel, 3rd Edition*, Daniel P. Bovet, Marco Cesati, O'Reilly.
3. Linux Kernel 2.6.19 source code.