7


下面是针对tasklet_init和tasklet_schedule的讲解

extern irq_cpustat_t irq_stat[];		/* defined in asm/hardirq.h */
#define __IRQ_STAT(cpu, member)	(irq_stat[cpu].member)
#endif

  /* arch independent irq_stat fields */
#define local_softirq_pending() \
	__IRQ_STAT(smp_processor_id(), __softirq_pending)
#define or_softirq_pending(x)  (local_softirq_pending() |= (x))

void __raise_softirq_irqoff(unsigned int nr)
{
	trace_softirq_raise(nr);
	or_softirq_pending(1UL << nr);
}


inline void raise_softirq_irqoff(unsigned int nr)
{
	__raise_softirq_irqoff(nr);

	/*
	 * If we're in an interrupt or softirq, we're done
	 * (this also catches softirq-disabled code). We will
	 * actually run the softirq once we return from
	 * the irq or softirq.
	 *
	 * Otherwise we wake up ksoftirqd to make sure we
	 * schedule the softirq soon.
	 */
	if (!in_interrupt())
		wakeup_softirqd();
}

void __tasklet_schedule(struct tasklet_struct *t)
{
	unsigned long flags;

	local_irq_save(flags);
	t->next = NULL;
	*__this_cpu_read(tasklet_vec.tail) = t;
	__this_cpu_write(tasklet_vec.tail, &(t->next));
	raise_softirq_irqoff(TASKLET_SOFTIRQ);
	local_irq_restore(flags);
}

static inline void tasklet_schedule(struct tasklet_struct *t)
{
	if (!test_and_set_bit(TASKLET_STATE_SCHED, &t->state))
		__tasklet_schedule(t);
}


void tasklet_init(struct tasklet_struct *t,
		  void (*func)(unsigned long), unsigned long data)
{
	t->next = NULL;
	t->state = 0;
	atomic_set(&t->count, 0);
	t->func = func;
	t->data = data;
}


demo:

# include <linux/kernel.h>
# include <linux/init.h>
# include <linux/module.h>
# include <linux/interrupt.h>

static int irq;
static char * devname;

module_param(irq,int,0644);
module_param(devname,charp,0644);

struct myirq
{
	int devid;
};

struct myirq mydev={1119};

static struct tasklet_struct mytasklet;
 
//中断下半部处理函数
static void mytasklet_handler(unsigned long data)
{
	printk("I am mytasklet_handler");
}

//中断处理函数
static irqreturn_t myirq_handler(int irq,void * dev)
{
	static int count=0;
	printk("count:%d\n",count+1);
	printk("I am myirq_handler\n");
	printk("The most of the interrupt work will be done by following tasklet\n");
	tasklet_init(&mytasklet,mytasklet_handler,0);	
	tasklet_schedule(&mytasklet);	
	count++;
	return IRQ_HANDLED;
}


//内核模块初始化函数
static int __init myirq_init(void)
{
	printk("Module is working...\n");
	if(request_irq(irq,myirq_handler,IRQF_SHARED,devname,&mydev)!=0)
	{
		printk("%s request IRQ:%d failed..\n",devname,irq);
		return -1;
	}
	printk("%s request IRQ:%d success...\n",devname,irq);
	return 0;
}

//内核模块退出函数
static void __exit myirq_exit(void)
{
	printk("Module is leaving...\n");
	free_irq(irq,&mydev);
	tasklet_kill(&mytasklet);
	printk("Free the irq:%d..\n",irq);
}

MODULE_LICENSE("GPL");
module_init(myirq_init);
module_exit(myirq_exit);