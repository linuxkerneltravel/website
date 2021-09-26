---
title: "open系统调用（一）"
date: 2021-01-25T17:41:29+08:00
author: "孙张品"
keywords: ["文件系统","系统调用"]
categories : ["系统调用"]
banner : "img/blogimg/ljrimg22.jpg"
summary : "本文对open系统调用执行流所涉及的内核函数作简单分析。用户进程在能够读/写一个文件之前必须要先“打开”这个文件。对文件的读/写从概念上说是一种进程与文件系统之间的一种“有连接”通信，所谓“打开文件”实质上就是在进程与文件之间建立起链接。"
---

# 1.前言

用户进程在能够读/写一个文件之前必须要先“打开”这个文件。对文件的读/写从概念上说是一种进程与文件系统之间的一种“有连接”通信，所谓“打开文件”实质上就是在进程与文件之间建立起链接。在文件系统的处理中，每当一个进程重复打开同一个文件时就建立起一个由file数据结构代表的独立的上下文。通常，一个file数据结构，即一个读/写文件的上下文，都由一个打开文件号（fd）加以标识。

# 2. do_sys_open

打开文件的系统调用是open()，在内核中通过do_sys_open()实现，其代码在 fs/open.c中:

```c

long do_sys_open(int dfd, const char __user *filename, int flags, int mode)
{
	struct open_flags op;
        //检查给定的 flags 参数是否有效，并处理不同的 flags 和 mode 条件。
	int lookup = build_open_flags(flags, mode, &op);
        //将文件路径从用户空间拷贝到内核空间。
	char *tmp = getname(filename);

	int fd = PTR_ERR(tmp);

	if (!IS_ERR(tmp)) {
                //从当前进程的打开文件表（files_struct）中找到一个空闲的表项，该表项的下标即为“打开文件号”。
		fd = get_unused_fd_flags(flags);
		if (fd >= 0) {
            //创建进程与文件的链接，或者说创建file结构体代表该读写文件的上下文。
			struct file *f = do_filp_open(dfd, tmp, &op, lookup);
			if (IS_ERR(f)) {
                //如果发生了错误，释放已分配的 fd 文件描述符
				put_unused_fd(fd);
                //释放已分配的 struct file 
				fd = PTR_ERR(f);
			} else {
				fsnotify_open(f);
                            //将这个file结构体的指针填入当前进程的打开文件表中。
				fd_install(fd, f);
			}
		}
		putname(tmp);
	}
	return fd;
}



pathname:代表需要打开的文件的文件名；

flags：表示打开的标识；

O_ACCMODE<0003>: 读写文件操作时，用于取出flag的低2位。
O_RDONLY<00>: 只读打开
O_WRONLY<01>: 只写打开
O_RDWR<02>: 读写打开
O_CREAT<0100>: 文件不存在则创建，需要mode_t
O_EXCL<0200>: 如果同时指定了O_CREAT，而文件已经存在，则出错
O_NOCTTY<0400>: 如果pathname代表终端设备，则不将此设备分配作为此进程的控制终端
O_TRUNC<01000>: 如果此文件存在，而且为只读或只写成功打开，则将其长度截短为0  
O_APPEND<02000>: 每次写时都加到文件的尾端
O_NONBLOCK<04000>: 如果p a t h n a m e指的是一个F I F O、一个块特殊文件或一个字符特殊文件，则此选择项为此文件的本次打开操作和后续的I / O操作设置非阻塞方式。
O_NDELAY<O_NONBLOCK>
O_SYNC<010000>: 使每次write都等到物理I/O操作完成。
FASYNC<020000>: 兼容BSD的fcntl同步操作
O_DIRECT<040000>: 直接磁盘操作标识，每次读写都不使用内核提供的缓存，直接读写磁盘设备
O_LARGEFILE<0100000>: 大文件标识
O_DIRECTORY<0200000>: 必须是目录
O_NOFOLLOW<0400000>: 不获取连接文件
O_NOATIME<01000000>: 暂无

mode:当新创建一个文件时，需要指定mode参数，以下说明的格式如宏定义名称<实际常数值>: 描述如下：

S_IRWXU<00700>：文件拥有者有读写执行权限
S_IRUSR (S_IREAD)<00400>：文件拥有者仅有读权限
S_IWUSR (S_IWRITE)<00200>：文件拥有者仅有写权限
S_IXUSR (S_IEXEC)<00100>：文件拥有者仅有执行权限
S_IRWXG<00070>：组用户有读写执行权限
S_IRGRP<00040>：组用户仅有读权限
S_IWGRP<00020>：组用户仅有写权限
S_IXGRP<00010>：组用户仅有执行权限
S_IRWXO<00007>：其他用户有读写执行权限
S_IROTH<00004>：其他用户仅有读权限
S_IWOTH<00002>：其他用户仅有写权限
S_IXOTH<00001>：其他用户仅有执行权限

```

dfd是传入的AT_FDCWD，其定义在 include/uapi/linux/fcntl.h。该值表明当 filename 为相对路径的情况下将当前进程的工作目录设置为起始路径。相对而言， 你可以在另一个系统调用 openat 中为这个起始路径指定一个目录， 此时 AT_FDCWD 就会被该目录的描述符所替代。


进程的task_struct结构中有个指针files，指向本进程的files_struct数据结构。与打开文件有关的信息都保存在这个数据结构中，其定义在include/linux/sched.h 中:
```c
struct files_struct {
atomic_t count;//引用计数
struct fdtable __rcu *fdt;//指向自身成员fdtab或动态分配的struct fdtable实例
struct fdtable fdtab;//当可打开文件的最大数目为NR_OPEN_DEFAULT时使用


spinlock_t file_lock ____cacheline_aligned_in_smp;
int next_fd;//已分配的描述符加1
struct embedded_fd_set close_on_exec_init;//位图，比特位数目刚好与NR_OPEN_DEFAULT一致
struct embedded_fd_set open_fds_init;
struct file __rcu * fd_array[NR_OPEN_DEFAULT];

};

struct fdtable {
unsigned int max_fds;//当前可打开文件的最大数目，决定struct file指针数组fd的大小，close_on_exec和open_fds位图的比特位总数
struct file __rcu **fd;//指向内置它的struct files_struct结构体的成员fd_array或动态分配的struct file指针数组
fd_set *close_on_exec;//指向内置它的struct files_struct结构体的成员close_on_exec_init或动态分配的位图
fd_set *open_fds;//指向内置它的struct files_struct结构体的成员open_fds_init或动态分配的位图
   //后两个成员在销毁时使用
   struct rcu_head rcu;    //rcu机制
   struct fdtable *next;    //用于加入fdtable_defer_list链表
};

```

其结构如下图所示。

![在这里插入图片描述](https://img-blog.csdnimg.cn/20201126151934968.png?x-oss-process=image,type_ZmFuZ3poZW5naGVpdGk,shadow_10,text_aHR0cHM6Ly9ibG9nLmNzZG4ubmV0L3FxXzMzMDk1NzMz,size_16,color_FFFFFF,t_70#pic_center)

# 3.do_filp_open

sys_do_open主要调用的函数为do_filp_open。

```c

struct file * do_filp_open(int dfd, struct filename * pathname,
	const struct open_flags * op) {
	struct nameidata nd;
	int flags = op->lookup_flags;
	struct file * filp;
	set_nameidata(&nd, dfd, pathname);
	filp = path_openat(&nd, op, flags | LOOKUP_RCU);

	if (unlikely(filp == ERR_PTR(-ECHILD)))
		filp = path_openat(&nd, op, flags);

	if (unlikely(filp == ERR_PTR(-ESTALE)))
		filp = path_openat(&nd, op, flags | LOOKUP_REVAL);

	restore_nameidata();
	return filp;
}


```


参数 dfd 是相对路径的基准目录对应的文件描述符，参数 name 指向文件路径，参数 op 是查找标志。=
在路径查找中有个很重要的数据结构 nameidata 用来向解析函数传递参数，保存解析结果。

```c

struct nameidata {
	struct path path;
	struct qstr last;
	struct path root;
	struct inode * inode; //path.dentry.d_inode
	unsigned int flags;
	unsigned seq, m_seq;
	int last_type;
	unsigned depth;
	int total_link_count;


	struct saved {
		struct path link;
		void * cookie;
		const char * name;
		struct inode * inode;
		unsigned seq;
	} * stack, internal[EMBEDDED_LEVELS];


	struct filename * name;
	struct nameidata * saved;
	unsigned root_seq;
	int dfd;
};


```

成员 last 存放需要解析的文件路径的分量（以前提到的组件）,是一个快速字符串(quick string),
不仅包字符串,还包含长度和散列值。
成员 path 存放解析得到的挂载描述符和目录项,成员 iode 存放目录项对应的索引节点。
path 保存已经成功解析到的信息，last 用来存放当前需要解析的信息，如果 last 解析成功
那么就会更新 path。


函数 do_flp_open 三次调用函数 path_openat以解析文件路径。

+ 第一次解析传入标志 LOOKUP_RCU,使用 RCU 查找(rcu-walk)方式。在散列表中根据{父目录, 名称}查找目录的过程中,使用 RCU 保护散列桶的链表,使用序列号保护目录，其他处理器可以并行地修改目录, RCU 查找方式速度最快。


+ 如果在第一次解析的过程中发现其他处理器修改了正在查找的目录,返回错误号-ECHILD，那么第二次使用引用查找（ref-walk）REF 方式，在散列表中根据{父目录, 名称}查找目录的过程中,使用 RCU 保护散列桶的链表,使用自旋锁保护目录，并且把目录的引用计数加1。引用查找方式速度比较慢。


+ 网络文件系统的文件在网络的服务器上，本地上次查询得到的信息可能过期,和服务器的当前状态不一致。如果第二次解析发现信息过期，返回错误号 -ESTALE，那么第三次解析传入标志 LOOKUP_REVAL，表示需要重新确认信息是否有效。
  
调用 set_nameidata() 保护当前进程现场信息。 接着调用 filp = path_openat(&nd, op, flags | LOOKUP_RCU);

# 4.path_openat

```c

static struct file * path_openat(struct nameidata * nd,
	const struct open_flags * op, unsigned flags) {
	const char * s;
	struct file * file;
	int opened = 0;
	int error;
  	// 获取一个空的 file 描述符。
	file = get_empty_filp();

  	//获取失败，返回。
	if (IS_ERR(file))
		return file;
  	//设置 file 描述符的查找标志。
	file->f_flags = op->open_flag;

  	// 如果是本次目标是创建一个临时文件，这里就不深入了，只研究正常的文件打开操作。
	if (unlikely(file->f_flags & __ O_TMPFILE)) {
		error = do_tmpfile(nd, flags, op, file, &opened);
		goto out2;
	}
  	// 路径初始化，确定查找的起始目录，初始化结构体 nameidata 的成员 path。
	s = path_init(nd, flags);

	//如果获取的路径（待查找的路径）无效，则释放 file 描述符，返回错误。
	if (IS_ERR(s)) {
		put_filp(file);
		return ERR_CAST(s);
	}
  	// 调用函数 link_path_walk 解析文件路径的每个分量，最后一个分量除外。
  	// 调用函数 do_last，解析文件路径的最后一个分量，并且打开文件。
	while (! (error = link_path_walk(s, nd)) && (error = do_last(nd, file, op, &opened)) > 0) {
		nd->flags &= ~(LOOKUP_OPEN | LOOKUP_CREATE | LOOKUP_EXCL);
    	// 如果最后一个分量是符号链接，调用 trailing_symlink 函数进行处理
    	// 读取符号链接文件的数据，新的文件路径是符号链接链接文件的数据，然后继续 while
    	// 循环，解析新的文件路径。
		s = trailing_symlink(nd);

		if (IS_ERR(s)) {
			error = PTR_ERR(s);
			break;
		}
	}
  	// 结束查找，释放解析文件路径的过程中保存的目录项和挂载描述符。
	terminate_walk(nd);

out2:

	if (! (opened & FILE_OPENED)) {
		BUG_ON(!error);
		put_filp(file);
	}

	if (unlikely(error)) {
		if (error == -EOPENSTALE) {
			if (flags & LOOKUP_RCU)
				error = -ECHILD;
			else
				error = -ESTALE;
		}

		file = ERR_PTR(error);
	}

	return file;
}


```