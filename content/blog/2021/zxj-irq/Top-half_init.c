3
内容：
主要给大家讲。。。

问题：irq_desc中断线的处理函数的是在什么地方注册的？
static inline void generic_handle_irq_desc(struct irq_desc *desc)
{
	desc->handle_irq(desc);
}


解：


static void
__irq_do_set_handler(struct irq_desc *desc, irq_flow_handler_t handle,
		     int is_chained, const char *name)
{
	if (!handle) {
		handle = handle_bad_irq;
	} else {
		struct irq_data *irq_data = &desc->irq_data;
#ifdef CONFIG_IRQ_DOMAIN_HIERARCHY
		/*
		 * With hierarchical domains we might run into a
		 * situation where the outermost chip is not yet set
		 * up, but the inner chips are there.  Instead of
		 * bailing we install the handler, but obviously we
		 * cannot enable/startup the interrupt at this point.
		 */
		while (irq_data) {
			if (irq_data->chip != &no_irq_chip)
				break;
			/*
			 * Bail out if the outer chip is not set up
			 * and the interrrupt supposed to be started
			 * right away.
			 */
			if (WARN_ON(is_chained))
				return;
			/* Try the parent */
			irq_data = irq_data->parent_data;
		}
#endif
		if (WARN_ON(!irq_data || irq_data->chip == &no_irq_chip))
			return;
	}

	/* Uninstall? */
	if (handle == handle_bad_irq) {
		if (desc->irq_data.chip != &no_irq_chip)
			mask_ack_irq(desc);
		irq_state_set_disabled(desc);
		if (is_chained)
			desc->action = NULL;
		desc->depth = 1;
	}
	desc->handle_irq = handle;
	desc->name = name;

	if (handle != handle_bad_irq && is_chained) {
		unsigned int type = irqd_get_trigger_type(&desc->irq_data);

		/*
		 * We're about to start this interrupt immediately,
		 * hence the need to set the trigger configuration.
		 * But the .set_type callback may have overridden the
		 * flow handler, ignoring that we're dealing with a
		 * chained interrupt. Reset it immediately because we
		 * do know better.
		 */
		if (type != IRQ_TYPE_NONE) {
			__irq_set_trigger(desc, type);
			desc->handle_irq = handle;
		}

		irq_settings_set_noprobe(desc);
		irq_settings_set_norequest(desc);
		irq_settings_set_nothread(desc);
		desc->action = &chained_action;
		irq_activate_and_startup(desc, IRQ_RESEND);
	}
}


void
__irq_set_handler(unsigned int irq, irq_flow_handler_t handle, int is_chained,
		  const char *name)
{
	unsigned long flags;
	struct irq_desc *desc = irq_get_desc_buslock(irq, &flags, 0);

	if (!desc)
		return;

	__irq_do_set_handler(desc, handle, is_chained, name);
	irq_put_desc_busunlock(desc, flags);
}

static void mp_register_handler(unsigned int irq, unsigned long trigger)
{
	irq_flow_handler_t hdl;
	bool fasteoi;

	if (trigger) {
		irq_set_status_flags(irq, IRQ_LEVEL);
		fasteoi = true;
	} else {
		irq_clear_status_flags(irq, IRQ_LEVEL);
		fasteoi = false;
	}

	hdl = fasteoi ? handle_fasteoi_irq : handle_edge_irq;
	__irq_set_handler(irq, hdl, 0, fasteoi ? "fasteoi" : "edge");
}