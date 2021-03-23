---
title: "Linux内核中的循环缓冲区"
date: 2020-08-02T6:20:43+08:00
author: "作者：王聪 编辑：崔鹏程"
keywords: ["循环缓冲区"]
categories : ["经验交流"]
banner : "img/blogimg/cpc1.jpg"
summary : "Linux内核中的循环缓冲区（circular buffer）为解决某些特殊情况下的竞争问题提供了一种免锁的方法。这种特殊的情况就是当生产者和消费者都只有一个，而在其它情况下使用它也是必须要加锁的。"
---

## Linux内核中的循环缓冲区


**作者：西邮 王聪**

Linux内核中的循环缓冲区（circular buffer）为解决某些特殊情况下的竞争问题提供了一种免锁的方法。这种特殊的情况就是当生产者和消费者都只有一个，而在其它情况下使用它也是必须要加锁的。


循环缓冲区定义在include/linux/kfifo.h中，如下：


```c
struct kfifo {

unsigned char *buffer;

unsigned int size;

unsigned int in;

unsigned int out;

spinlock_t *lock;

};
```

buffer指向存放数据的缓冲区，size是缓冲区的大小，in是写指针下标，out是读指针下标，lock是加到struct kfifo上的自旋锁（上面说的免锁不是免这里的锁，这个锁是必须的），防止多个进程并发访问此数据结构。当in==out时，说明缓冲区为空；当(in-out)==size时，说明缓冲区已满。


为kfifo提供的接口可以分为两类，一类是满足上述情况下使用的，以双下划线开头，没有加锁的；另一类是在不满足的条件下，即需要额外加锁的情况下使用的。其实后一类只是在前一类的基础上进行加锁后的包装（也有一处进行了小的改进），实现中所加的锁是spin_lock_irqsave。


清空缓冲区的函数：
static inline void __kfifo_reset(struct kfifo *fifo);
static inline void kfifo_reset(struct kfifo *fifo);

这很简单，直接把读写指针都置为0即可。

向缓冲区里放入数据的接口是：
static inline unsigned int kfifo_put(struct kfifo *fifo, unsigned char *buffer, unsigned int len);
unsigned int __kfifo_put(struct kfifo *fifo, unsigned char *buffer, unsigned int len);


后者是在kernel/kfifo.c中定义的。这个接口是经过精心构造的，可以小心地避免一些边界情况。我们有必要一起来看一下它的具体实现。

```c
1 unsigned int __kfifo_put(struct kfifo *fifo,

2 unsigned char *buffer, unsigned int len)

3 {

4 unsigned int l;

5

6 len = min(len, fifo->size - fifo->in + fifo->out);

...

13 smp_mb();

14

15 /* first put the data starting from fifo->in to buffer end */

16 l = min(len, fifo->size - (fifo->in & (fifo->size - 1)));

17 memcpy(fifo->buffer + (fifo->in & (fifo->size - 1)), buffer, l);

18

19 /* then put the rest (if any) at the beginning of the buffer */

20 memcpy(fifo->buffer, buffer + l, len - l);

...

27 smp_wmb();

28

29 fifo->in += len;

30

31 return len;

32 }
```

第6行，在len和(fifo->size - fifo->in + fifo->out)之间取一个较小的值赋给len。注意，当(fifo->in == fifo->out+fifo->size)时，表示缓冲区已满，此时得到的较小值一定是0，后面实际写入的字节数也全为0。另一种边界情况是当len很大时（因为len是无符号的，负数对它来说也是一个很大的正数），这一句也能保证len取到一个较小的值，因为fifo->in总是大于等于fifo->out，所以后面的那个表达式的值不会超过fifo->size的大小。


第13行和第27行是加内存屏障，这里不是我们讨论的范围，你可以忽略它。


第16行是把上一步决定的要写入的字节数len“切开”，这里又使用了一个技巧。注意：实际分配给fifo->buffer的字节数fifo->size，必须是2的幂，否则这里就会出错。既然fifo->size是2的幂，那么(fifo->size-1)也就是一个后面几位全为1的数，也就能保证(fifo->in & (fifo->size - 1))总为不超过(fifo->size - 1)的那一部分，和(fifo->in)% (fifo->size - 1)的效果一样。


这样后面的代码就不难理解了，它先向fifo->in到缓冲区末端这一块写数据，如果还没写完，在从缓冲区头开始写入剩下的，从而实现了循环缓冲。最后，把写指针后移len个字节，并返回len。


从上面可以看出，fifo->in的值可以从0变化到超过fifo->size的数值，fifo->out也如此，但它们的差不会超过fifo->size。


从kfifo向外读数据的函数是：


static inline unsigned int kfifo_get(struct kfifo *fifo, unsigned char *buffer, unsigned int len);

unsigned int __kfifo_get(struct kfifo *fifo, unsigned char *buffer, unsigned int len);


和上面的__kfifo_put类似，不难分析。


static inline unsigned int __kfifo_len(struct kfifo *fifo);

static inline unsigned int kfifo_len(struct kfifo *fifo);


这两个函数返回缓冲区中实际的字节数，只要用fifo->in减去fifo->out即可。


kernel/kfifo.c中还提供了初始化kfifo，分配和释放kfifo的接口：


struct kfifo *kfifo_init(unsigned char *buffer, unsigned int size, gfp_t gfp_mask, spinlock_t *lock);

struct kfifo *kfifo_alloc(unsigned int size, gfp_t gfp_mask, spinlock_t *lock);

void kfifo_free(struct kfifo *fifo);


再一次强调，调用kfifo_init必须保证size是2的幂，而kfifo_alloc不必，它内部会把size向上圆到2的幂。kfifo_alloc和kfifo_free搭配使用，因为这两个函数会为fifo->buffer分配/释放内存空间。而kfifo_init只会接受一个已分配好空间的fifo->buffer，不能和kfifo->free搭配，用kfifo_init分配的kfifo只能用kfree释放。


循环缓冲区在驱动程序中使用较多，尤其是网络适配器。但这种免锁的方式在内核互斥中使用较少，取而代之的是另一种高级的互斥机制──RCU。


> 参考资料：
> 1. Linux Device Drivers, 3rd Edition, Jonathan Corbet, Alessandro Rubini and Greg Kroah-Hartman, O'Reilly.
> 2. Linux Kernel 2.6.19 source code.
