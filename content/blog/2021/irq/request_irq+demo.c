4

首先看下demo，然后在分析：
static int
__setup_irq(unsigned int irq, struct irq_desc *desc, struct irqaction *new)
{
    struct irqaction *old, **old_ptr;
    ...

    new->irq = irq;

    ...

    raw_spin_lock_irqsave(&desc->lock, flags);
    old_ptr = &desc->action;
    old = *old_ptr;
    if (old) {
        /*
         * Can't share interrupts unless both agree to and are
         * the same type (level, edge, polarity). So both flag
         * fields must have IRQF_SHARED set and the bits which
         * set the trigger type must match. Also all must
         * agree on ONESHOT.
         */
        unsigned int oldtype;

        /*
         * If nobody did set the configuration before, inherit
         * the one provided by the requester.
         */
        if (irqd_trigger_type_was_set(&desc->irq_data)) {
            oldtype = irqd_get_trigger_type(&desc->irq_data);
        } else {
            oldtype = new->flags & IRQF_TRIGGER_MASK;
            irqd_set_trigger_type(&desc->irq_data, oldtype);
        }

        if (!((old->flags & new->flags) & IRQF_SHARED) ||
            (oldtype != (new->flags & IRQF_TRIGGER_MASK)) ||
            ((old->flags ^ new->flags) & IRQF_ONESHOT))
            goto mismatch;

        /* All handlers must agree on per-cpuness */
        if ((old->flags & IRQF_PERCPU) !=
            (new->flags & IRQF_PERCPU))
            goto mismatch;

        /* add new interrupt at end of irq queue */
        do {
            /*
             * Or all existing action->thread_mask bits,
             * so we can find the next zero bit for this
             * new action.
             */
            thread_mask |= old->thread_mask;
            old_ptr = &old->next;
            old = *old_ptr;
        } while (old);
        shared = 1;
    }

    ...
}

int request_threaded_irq(unsigned int irq, irq_handler_t handler,
             irq_handler_t thread_fn, unsigned long irqflags,
             const char *devname, void *dev_id)
{
    struct irqaction *action;
    struct irq_desc *desc;
    int retval;

    if (irq == IRQ_NOTCONNECTED)
        return -ENOTCONN;

    /*
     * Sanity-check: shared interrupts must pass in a real dev-ID,
     * otherwise we'll have trouble later trying to figure out
     * which interrupt is which (messes up the interrupt freeing
     * logic etc).
     *
     * Also IRQF_COND_SUSPEND only makes sense for shared interrupts and
     * it cannot be set along with IRQF_NO_SUSPEND.
     */
    if (((irqflags & IRQF_SHARED) && !dev_id) ||
        (!(irqflags & IRQF_SHARED) && (irqflags & IRQF_COND_SUSPEND)) ||
        ((irqflags & IRQF_NO_SUSPEND) && (irqflags & IRQF_COND_SUSPEND)))
        return -EINVAL;

    desc = irq_to_desc(irq);
    if (!desc)
        return -EINVAL;

    if (!irq_settings_can_request(desc) ||
        WARN_ON(irq_settings_is_per_cpu_devid(desc)))
        return -EINVAL;

    if (!handler) {
        if (!thread_fn)
            return -EINVAL;
        handler = irq_default_primary_handler;
    }

    action = kzalloc(sizeof(struct irqaction), GFP_KERNEL);
    if (!action)
        return -ENOMEM;

    action->handler = handler;
    action->thread_fn = thread_fn;
    action->flags = irqflags;
    action->name = devname;
    action->dev_id = dev_id;

    retval = irq_chip_pm_get(&desc->irq_data);
    if (retval < 0) {
        kfree(action);
        return retval;
    }

    retval = __setup_irq(irq, desc, action);

    if (retval) {
        irq_chip_pm_put(&desc->irq_data);
        kfree(action->secondary);
        kfree(action);
    }

#ifdef CONFIG_DEBUG_SHIRQ_FIXME
    if (!retval && (irqflags & IRQF_SHARED)) {
        /*
         * It's a shared IRQ -- the driver ought to be prepared for it
         * to happen immediately, so let's make sure....
         * We disable the irq to make sure that a 'real' IRQ doesn't
         * run in parallel with our fake.
         */
        unsigned long flags;

        disable_irq(irq);
        local_irq_save(flags);

        handler(irq, dev_id);

        local_irq_restore(flags);
        enable_irq(irq);
    }
#endif
    return retval;
}

接口的原型：
static inline int __must_check
request_irq(unsigned int irq, irq_handler_t handler, unsigned long flags,
        const char *name, void *dev)
{
    return request_threaded_irq(irq, handler, NULL, flags, name, dev);
}




demo：
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
		
//中断处理函数
static irqreturn_t myirq_handler(int irq,void * dev)
{
    struct myirq mydev;
    static int count=1;
    mydev = *(struct myirq*)dev;		
    printk("key: %d..\n",count);
    printk("devid:%d ISR is working..\n",mydev.devid);
    printk("ISR is leaving......\n");
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
    printk("Free the irq:%d..\n",irq);
}

MODULE_LICENSE("GPL");
module_init(myirq_init);
module_exit(myirq_exit);