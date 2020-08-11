---
title: "TCP超时重传定时器"
date: 2020-08-11T16:05:20+08:00
author: "作者：张孝家"
keywords: ["定时器","超时重传"]
categories : ["内核网络"]
banner : "img/blogimg/zxj0.jpg"
summary : "本篇文章主要介绍定时器的创建、删除、激活和超时处理回调函数等内容，不介绍关于RTO和RTT的计算（后期的文章在补充），使用的内核版本为4.15"
---

TCP协议是一种面向连接的可靠的传输层协议，它保证了数据的可靠传输，对于一些出错，超时丢包等问题TCP设计的超时与重传机制。其基本原理：在发送一个数据之后，就开启一个定时器，若是在这个时间内没有收到发送数据的ACK确认报文，则对该报文进行重传，在达到一定次数还没有成功时放弃并发送一个复位信号。

说明：本篇文章主要介绍定时器的创建、删除、激活和超时处理回调函数等内容，不介绍关于RTO和RTT的计算（后期的文章在补充）。

### 创建

```
流程图
sk->sk_prot->init --指向--> tcp_v4_init_sock(这里以tcp v4 为例)
	|--->tcp_init_sock
		   |--->tcp_init_xmit_timers
				   |----->inet_csk_init_xmit_timers
				   |	       |------timer_setup
				   |----->hrtimer_init

源码分析：
//tcp的sock的初始化的工作，其中主要分析了关于定时器的初始化设置。
static int tcp_v4_init_sock(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	tcp_init_sock(sk);

	icsk->icsk_af_ops = &ipv4_specific;

#ifdef CONFIG_TCP_MD5SIG
	tcp_sk(sk)->af_specific = &tcp_sock_ipv4_specific;
#endif

	return 0;
}

//这个主要做tcp_sock的初始化的工作，
//然后调用tcp_init_xmit_timers函数进行定时器的初始化的工作。
void tcp_init_sock(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	struct tcp_sock *tp = tcp_sk(sk);

	tp->out_of_order_queue = RB_ROOT;
	sk->tcp_rtx_queue = RB_ROOT;
	tcp_init_xmit_timers(sk);
	INIT_LIST_HEAD(&tp->tsq_node);
	INIT_LIST_HEAD(&tp->tsorted_sent_queue);

	icsk->icsk_rto = TCP_TIMEOUT_INIT;
	tp->mdev_us = jiffies_to_usecs(TCP_TIMEOUT_INIT);
	minmax_reset(&tp->rtt_min, tcp_jiffies32, ~0U);

	/* So many TCP implementations out there (incorrectly) count the
	 * initial SYN frame in their delayed-ACK and congestion control
	 * algorithms that we must have the following bandaid to talk
	 * efficiently to them.  -DaveM
	 */
	tp->snd_cwnd = TCP_INIT_CWND;

	/* There's a bubble in the pipe until at least the first ACK. */
	tp->app_limited = ~0U;

	/* See draft-stevens-tcpca-spec-01 for discussion of the
	 * initialization of these values.
	 */
	tp->snd_ssthresh = TCP_INFINITE_SSTHRESH;
	tp->snd_cwnd_clamp = ~0;
	tp->mss_cache = TCP_MSS_DEFAULT;

	tp->reordering = sock_net(sk)->ipv4.sysctl_tcp_reordering;
	tcp_assign_congestion_control(sk);

	tp->tsoffset = 0;
	tp->rack.reo_wnd_steps = 1;

	sk->sk_state = TCP_CLOSE;

	sk->sk_write_space = sk_stream_write_space;
	sock_set_flag(sk, SOCK_USE_WRITE_QUEUE);

	icsk->icsk_sync_mss = tcp_sync_mss;

	sk->sk_sndbuf = sock_net(sk)->ipv4.sysctl_tcp_wmem[1];
	sk->sk_rcvbuf = sock_net(sk)->ipv4.sysctl_tcp_rmem[1];

	sk_sockets_allocated_inc(sk);
}
EXPORT_SYMBOL(tcp_init_sock);

//这个函数主要是调用定时器的初始化函数，对各种定时器的回调函数进行赋值。
//如重传定时器的回调函数是：tcp_write_timer
//延时定时器的回调函数是：tcp_delack_timer
//保活定时器的回调函数是：tcp_keepalive_timer
void tcp_init_xmit_timers(struct sock *sk)
{
	inet_csk_init_xmit_timers(sk, &tcp_write_timer, &tcp_delack_timer,
				  &tcp_keepalive_timer);
	hrtimer_init(&tcp_sk(sk)->pacing_timer, CLOCK_MONOTONIC,
		     HRTIMER_MODE_ABS_PINNED);
	tcp_sk(sk)->pacing_timer.function = tcp_pace_kick;
}

//对inet_connection_sock中的定时器进行回调函数的赋值。
//调用函数timer_setup函数来实现这个功能。
void inet_csk_init_xmit_timers(struct sock *sk,
			       void (*retransmit_handler)(struct timer_list *t),
			       void (*delack_handler)(struct timer_list *t),
			       void (*keepalive_handler)(struct timer_list *t))
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	timer_setup(&icsk->icsk_retransmit_timer, retransmit_handler, 0);
	timer_setup(&icsk->icsk_delack_timer, delack_handler, 0);
	timer_setup(&sk->sk_timer, keepalive_handler, 0);
	icsk->icsk_pending = icsk->icsk_ack.pending = 0;
}
```



### 删除

```
流程图
sk->sk_prot->destroy --指向--> tcp_v4_destroy_sock(这里以tcp v4 为例)
      |--->tcp_clear_xmit_timers
		      |---->inet_csk_clear_xmit_timers
源码分析：
//这个函数将定时器相关的事件类型置0，和删除定时器。
void inet_csk_clear_xmit_timers(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	icsk->icsk_pending = icsk->icsk_ack.pending = icsk->icsk_ack.blocked = 0;

	sk_stop_timer(sk, &icsk->icsk_retransmit_timer);
	sk_stop_timer(sk, &icsk->icsk_delack_timer);
	sk_stop_timer(sk, &sk->sk_timer);
}
```

### 激活

```
/*函数功能：根据参数whant的事件类型，设置当前的套接字的定时器的超时时间。
 *参数：
 *	sk:	当前的sock套接字
 *	what:	定时器的时间类型
 *	when:	定时器设置的超时时间
 *	max_when:	定时器设置的最大超时时间
 */
static inline void inet_csk_reset_xmit_timer(struct sock *sk, const int what,
					     unsigned long when,
					     const unsigned long max_when)
{
	struct inet_connection_sock *icsk = inet_csk(sk);

	if (when > max_when) {
#ifdef INET_CSK_DEBUG
		pr_debug("reset_xmit_timer: sk=%p %d when=0x%lx, caller=%p\n",
			 sk, what, when, current_text_addr());
#endif
		when = max_when;
	}

	if (what == ICSK_TIME_RETRANS || what == ICSK_TIME_PROBE0 ||
	    what == ICSK_TIME_EARLY_RETRANS || what == ICSK_TIME_LOSS_PROBE ||
	    what == ICSK_TIME_REO_TIMEOUT) {
		icsk->icsk_pending = what;
		icsk->icsk_timeout = jiffies + when;
		sk_reset_timer(sk, &icsk->icsk_retransmit_timer, icsk->icsk_timeout);
	} else if (what == ICSK_TIME_DACK) {
		icsk->icsk_ack.pending |= ICSK_ACK_TIMER;
		icsk->icsk_ack.timeout = jiffies + when;
		sk_reset_timer(sk, &icsk->icsk_delack_timer, icsk->icsk_ack.timeout);
	}
#ifdef INET_CSK_DEBUG
	else {
		pr_debug("%s", inet_csk_timer_bug_msg);
	}
#endif
}
```



### 超时处理函数

```
流程图：
tcp_write_timer
   |---->tcp_write_timer_handler
             ---》switch（）
                  |---tcp_rack_reo_timeout
                  |---tcp_send_loss_probe
                  |---tcp_retransmit_timer
                  |		 （我们这边要分析的）
                  |---tcp_probe_timer
源码分析
/*	这边的超时函数就是我们在创建超时定时器赋值的回调函数。
inet_csk_init_xmit_timers(sk, &tcp_write_timer, 
							&tcp_delack_timer, &tcp_keepalive_timer);
 */
 //这个函数就是超时重传定时器发生超时的回调函数。
static void tcp_write_timer(struct timer_list *t)
{
	struct inet_connection_sock *icsk =
			from_timer(icsk, t, icsk_retransmit_timer);
	struct sock *sk = &icsk->icsk_inet.sk;

	bh_lock_sock(sk);
	if (!sock_owned_by_user(sk)) {
		tcp_write_timer_handler(sk);
	} else {
		/* delegate our work to tcp_release_cb() */
		if (!test_and_set_bit(TCP_WRITE_TIMER_DEFERRED, &sk->sk_tsq_flags))
			sock_hold(sk);
	}
	bh_unlock_sock(sk);
	sock_put(sk);
}

//这个函数根据当前的sock套接字超时的事件（icsk->icsk_pending），
//调用不同的函数进行处理。如：ICSK_TIME_RETRANS事件类型
//处理函数是：tcp_retransmit_timer。
void tcp_write_timer_handler(struct sock *sk)
{
	struct inet_connection_sock *icsk = inet_csk(sk);
	int event;

	if (((1 << sk->sk_state) & (TCPF_CLOSE | TCPF_LISTEN)) ||
	    !icsk->icsk_pending)
		goto out;

	if (time_after(icsk->icsk_timeout, jiffies)) {
		sk_reset_timer(sk, &icsk->icsk_retransmit_timer, icsk->icsk_timeout);
		goto out;
	}

	tcp_mstamp_refresh(tcp_sk(sk));
	event = icsk->icsk_pending;

	switch (event) {
	case ICSK_TIME_REO_TIMEOUT:
		tcp_rack_reo_timeout(sk);
		break;
	case ICSK_TIME_LOSS_PROBE:
		tcp_send_loss_probe(sk);
		break;
	case ICSK_TIME_RETRANS:
		icsk->icsk_pending = 0;
		tcp_retransmit_timer(sk);
		break;
	case ICSK_TIME_PROBE0:
		icsk->icsk_pending = 0;
		tcp_probe_timer(sk);
		break;
	}

out:
	sk_mem_reclaim(sk);
}

//tcp_retransmit_timer函数即为超时重传的核心函数，其根据不同的情况决定是否进行重传，
//并且调整重传次数和退避指数，设定下一次重传定时器等；
void tcp_retransmit_timer(struct sock *sk)
{
	struct tcp_sock *tp = tcp_sk(sk);
	struct net *net = sock_net(sk);
	struct inet_connection_sock *icsk = inet_csk(sk);
	//这个字段我还不清楚什么含义，这里暂且不分析。
	if (tp->fastopen_rsk) {
		WARN_ON_ONCE(sk->sk_state != TCP_SYN_RECV &&
			     sk->sk_state != TCP_FIN_WAIT1);
		tcp_fastopen_synack_timer(sk);
		/* Before we receive ACK to our SYN-ACK don't retransmit
		 * anything else (e.g., data or FIN segments).
		 */
		return;
	}
	//如果没有发送且没有应答的数据包，就goto out;
	if (!tp->packets_out)
		goto out;

	WARN_ON(tcp_rtx_queue_empty(sk));

	tp->tlp_high_seq = 0;

	 /* 如果对端通告窗口为0，sk不处于DEAD状态，TCP连接不处于三次握手 */
	if (!tp->snd_wnd && !sock_flag(sk, SOCK_DEAD) &&
	    !((1 << sk->sk_state) & (TCPF_SYN_SENT | TCPF_SYN_RECV))) {
		/* Receiver dastardly shrinks window. Our retransmits
		 * become zero probes, but we should not timeout this
		 * connection. If the socket is an orphan, time it out,
		 * we cannot allow such beasts to hang infinitely.
		 */
		struct inet_sock *inet = inet_sk(sk);
		if (sk->sk_family == AF_INET) {
			net_dbg_ratelimited("Peer %pI4:%u/%u unexpectedly shrunk window %u:%u (repaired)\n",
					    &inet->inet_daddr,
					    ntohs(inet->inet_dport),
					    inet->inet_num,
					    tp->snd_una, tp->snd_nxt);
		}
#if IS_ENABLED(CONFIG_IPV6)
		else if (sk->sk_family == AF_INET6) {
			net_dbg_ratelimited("Peer %pI6:%u/%u unexpectedly shrunk window %u:%u (repaired)\n",
					    &sk->sk_v6_daddr,
					    ntohs(inet->inet_dport),
					    inet->inet_num,
					    tp->snd_una, tp->snd_nxt);
		}
#endif
		 /* 距离上次收到ACK的时间超过了最大超时时间，认为有错误发生了*/
		if (tcp_jiffies32 - tp->rcv_tstamp > TCP_RTO_MAX) {
			tcp_write_err(sk);/* 报告错误，调用tcp_done终止连接 */
			goto out;
		}
		/* 进入Loss状态，标志丢失的数据段 */
		tcp_enter_loss(sk);
		 /* 重传发送队列的第一个数据段 */
		tcp_retransmit_skb(sk, tcp_rtx_queue_head(sk), 1);
		/* 更新路由缓存 */
		__sk_dst_reset(sk);
		goto out_reset_timer;
	}

    /* 如果重传次数达到上限，报告错误并关闭套接口。
     * 如果资源使用达到上限，放弃本次重传。
     */
	if (tcp_write_timeout(sk))
		goto out;

	/* 刚进入超时重传，这个里面具体含义不是很清楚，关于一些数据的统计*/
	if (icsk->icsk_retransmits == 0) {
		int mib_idx;

		if (icsk->icsk_ca_state == TCP_CA_Recovery) {
			if (tcp_is_sack(tp))
				mib_idx = LINUX_MIB_TCPSACKRECOVERYFAIL;
			else
				mib_idx = LINUX_MIB_TCPRENORECOVERYFAIL;
		} else if (icsk->icsk_ca_state == TCP_CA_Loss) {
			mib_idx = LINUX_MIB_TCPLOSSFAILURES;
		} else if ((icsk->icsk_ca_state == TCP_CA_Disorder) ||
			   tp->sacked_out) {
			if (tcp_is_sack(tp))
				mib_idx = LINUX_MIB_TCPSACKFAILURES;
			else
				mib_idx = LINUX_MIB_TCPRENOFAILURES;
		} else {
			mib_idx = LINUX_MIB_TCPTIMEOUTS;
		}
		__NET_INC_STATS(sock_net(sk), mib_idx);
	}

	tcp_enter_loss(sk);

	/* 重传发送队列的第一个数据段，如果失败说明是本地拥塞，那么不进行指数退避 */
	if (tcp_retransmit_skb(sk, tcp_rtx_queue_head(sk), 1) > 0) {
		/* Retransmission failed because of local congestion,
		 * do not backoff.
		 */
		if (!icsk->icsk_retransmits)
			icsk->icsk_retransmits = 1;
		inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS,
					  min(icsk->icsk_rto, TCP_RESOURCE_PROBE_INTERVAL),
					  TCP_RTO_MAX);
		goto out;
	}

	/* Increase the timeout each time we retransmit.  Note that
	 * we do not increase the rtt estimate.  rto is initialized
	 * from rtt, but increases here.  Jacobson (SIGCOMM 88) suggests
	 * that doubling rto each time is the least we can get away with.
	 * In KA9Q, Karn uses this for the first few times, and then
	 * goes to quadratic.  netBSD doubles, but only goes up to *64,
	 * and clamps at 1 to 64 sec afterwards.  Note that 120 sec is
	 * defined in the protocol as the maximum possible RTT.  I guess
	 * we'll have to use something other than TCP to talk to the
	 * University of Mars.
	 *
	 * PAWS allows us longer timeouts and large windows, so once
	 * implemented ftp to mars will work nicely. We will have to fix
	 * the 120 second clamps though!
	 */
	icsk->icsk_backoff++;/* 退避指数，采集到新的RTT样本时清零 */
	icsk->icsk_retransmits++; /* 超时次数，当确认了新数据时清零 */

out_reset_timer:
	/* If stream is thin, use linear timeouts. Since 'icsk_backoff' is
	 * used to reset timer, set to 0. Recalculate 'icsk_rto' as this
	 * might be increased if the stream oscillates between thin and thick,
	 * thus the old value might already be too high compared to the value
	 * set by 'tcp_set_rto' in tcp_input.c which resets the rto without
	 * backoff. Limit to TCP_THIN_LINEAR_RETRIES before initiating
	 * exponential backoff behaviour to avoid continue hammering
	 * linear-timeout retransmissions into a black hole
	 */
	/*icsk->icsk_rto 超时时间的计算*/
	if (sk->sk_state == TCP_ESTABLISHED &&
	    (tp->thin_lto || net->ipv4.sysctl_tcp_thin_linear_timeouts) &&
	    tcp_stream_is_thin(tp) &&
	    icsk->icsk_retransmits <= TCP_THIN_LINEAR_RETRIES) {
		icsk->icsk_backoff = 0;
		icsk->icsk_rto = min(__tcp_set_rto(tp), TCP_RTO_MAX);/* RTO保持原样 */
	} else {
		/* Use normal (exponential) backoff */
		icsk->icsk_rto = min(icsk->icsk_rto << 1, TCP_RTO_MAX);/* RTO翻倍 */
	}
	/*超时重传定时器的超时时间的启动*/
	inet_csk_reset_xmit_timer(sk, ICSK_TIME_RETRANS, icsk->icsk_rto, TCP_RTO_MAX);
	/* 当重传多次时，需要根据重传时间间隔，决定是否更新路由缓存  */
	if (retransmits_timed_out(sk, net->ipv4.sysctl_tcp_retries1 + 1, 0))
		__sk_dst_reset(sk);

out:;
}
```

