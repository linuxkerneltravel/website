---
title: "open系统调用（二）"
date: 2021-01-25T17:41:44+08:00
author: "孙张品"
keywords: ["文件系统","系统调用"]
categories : ["系统调用"]
banner : "img/blogimg/ljrimg21.jpg"
summary : "open系统调用（一）中说到通过调用函数path_openat以解析文件路径，path_openat中包装了两个重要的函数path_init和link_path_walk，本文就从这两个函数开始，继续打开文件的旅程。"
---

# 1. 前言

前面说到通过调用函数path_openat以解析文件路径，path_openat中包装了两个重要的函数path_init和link_path_walk，本文就从这两个函数开始，继续打开文件的旅程。

# 2. path_opennat

再次看一下path_opennat函数，其中的path_init和link_path_walk通常连在一起调用，二者合在一起就可以根据给定的文件路径名称在内存中找到或者建立代表着目标文件或者目录的dentry结构和inode结构。



```c

static struct file * path_openat(struct nameidata * nd,
	...
  	// 路径初始化，确定查找的起始目录，初始化结构体 nameidata 的成员 path。
	s = path_init(nd, flags);
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
...
}


```

# 3.path_lookup

在内核版本2.6，已经使用path_lookup函数代替了path_init函数，实现的功能都是一样的，做查找前的准备工作，初始化nd，nd存储的是查找结果，nd就是nameidata结构体。具体实现如下：
```c

//做查找前的准备工作，初始化nd，nd存储的是查找结果。
int path_lookup(const char *name, unsigned int flags, struct nameidata *nd)
{
	nd->last_type = LAST_ROOT; /* if there are only slashes... */
	nd->flags = flags;

	read_lock(&current->fs->lock);
	if (*name=='/') {//如果是从根目录开始的绝对路径

		//当前进程没有使用chroot()系统调用设置替换根目录，altroot为0
		if (current->fs->altroot && !(nd->flags & LOOKUP_NOALT)) {
			
			/*flags中的LOOKUP_NOALT标志位通常为0，这些标志位都是对寻找目标的指示。
			既然替换过了根目录，那就赋值，替换后的根目录。
			*/
			nd->mnt = mntget(current->fs->altrootmnt);
			nd->dentry = dget(current->fs->altroot);
			read_unlock(&current->fs->lock);
			if (__emul_lookup_dentry(name,nd))
				return 0;
			read_lock(&current->fs->lock);
		}
		//没有替换根目录，直接赋值当前进程的根目录
		nd->mnt = mntget(current->fs->rootmnt);
		nd->dentry = dget(current->fs->root);
	}
	else{//从进程当前目录开始的相对路径
	
		//赋值指向的vfsmount
		nd->mnt = mntget(current->fs->pwdmnt);

		/*dentry初始化为当前目录，也就是搜索的起点，即从根目录到当前目录的那部分不用查找了。fs_struct->pwd存放当前目录dentry。
		dget函数将该dentry引用计数+1后返回。
		*/
		nd->dentry = dget(current->fs->pwd);
	}
	read_unlock(&current->fs->lock);
	current->total_link_count = 0;
	return link_path_walk(name, nd);
}

```
函数最后调用link_path_walk进行实际的搜索工作。

# 4.link_path_walk

path_lookup函数执行成功后，就会在nameidata结构体的成员dentry中指向搜索路径的起点，接下来就是link_path_walk函数顺着路径进行搜索了。

```c
int link_path_walk(const char * name, struct nameidata *nd)
{
	struct path next;
	struct inode *inode;
	int err;
	unsigned int lookup_flags = nd->flags;
	//如果是根目录，跳过‘/’
	while (*name=='/')
		name++;
	//如果路径只包含‘/’,搜索完成，返回。
	if (!*name)
		goto return_reval;
	
	//获取dentry对应的inode
	inode = nd->dentry->d_inode;

	//link_count计数器，记录查找链的长度，当达到某个值时，就终止搜索，防止陷入循环。

	if (current->link_count)//计数值不是0，则说明顺着符号链接，进入了另一个设备的文件系统，递归调用了本函数。
		//查找标志设置为1，表示顺着符号链接，找到终点。
		lookup_flags = LOOKUP_FOLLOW;

	/* At this point we know we have a real path component. */
	for(;;) {
		unsigned long hash;
		//存放当前节点的信息
		struct qstr this;
		unsigned int c;
		//检查是否拥有中间目录的权限，需要有执行权限MAY_EXEC，这里先使用permission函数的青春版进行检查，不通过，再使用原函数做更多检查
		err = exec_permission_lite(inode, nd);
		if (err == -EAGAIN) { 
			err = permission(inode, MAY_EXEC, nd);
		}
 		if (err)
			break;

		this.name = name;
		c = *(const unsigned char *)name;

		hash = init_name_hash();

		//逐个字符的计算出，当前节点名称的哈希值，遇到/或者‘\0’退出。
		do {
			name++;
			hash = partial_name_hash(c, hash);
			c = *(const unsigned char *)name;
		} while (c && (c != '/'));

		
		this.len = name - (const char *) this.name;
		this.hash = end_name_hash(hash);

		/* remove trailing slashes? */
		if (!c)//当前节点的最后一个字符是‘\0’,即为当前路径中的最后一节
			goto last_component;
		
		//当前节点名称的最后一个字符是/
		while (*++name == '/');

		//当前节点已经是最后一个，只不过后面加了‘/’
		if (!*name)
			goto last_with_slashes;

		/*
		 * "." and ".." are special - ".." especially so because it has
		 * to be able to know about the current root directory and
		 * parent relationships.
		 */
		 /*到这里节点一定是中间节点或者起始节点，并且是个目录*/
		if (this.name[0] == '.') switch (this.len) {
			//如果目录的第一个字符是‘.’，当前节点长度只能为1或者2
			default:
				break;
			case 2:	
				if (this.name[1] != '.')//如果是2，第二个字符也是‘.’
					break;
				//查找当前目录的父目录
				follow_dotdot(&nd->mnt, &nd->dentry);
				
				inode = nd->dentry->d_inode;
				/* fallthrough */
			case 1:
				//回到for循环开始，继续下一个节点
				continue;
		}
		/*
		 * See if the low-level filesystem might want
		 * to use its own hash..
		 */
		if (nd->dentry->d_op && nd->dentry->d_op->d_hash) {
			err = nd->dentry->d_op->d_hash(nd->dentry, &this);
			if (err < 0)
				break;
		}

		//准备工作完成，开始搜索
		nd->flags |= LOOKUP_CONTINUE;
		/* This does the actual lookups.. */
		//找到或者建立的所需的dentry结构
		err = do_lookup(nd, &this, &next);
		if (err)
			break;
		/* Check mountpoints.. */
		follow_mount(&next.mnt, &next.dentry);

		err = -ENOENT;
		inode = next.dentry->d_inode;
		if (!inode)
			goto out_dput;
		err = -ENOTDIR; 
		if (!inode->i_op)
			goto out_dput;

		if (inode->i_op->follow_link) {//判断当前节点是不是一个链接
			mntget(next.mnt);
			err = do_follow_link(next.dentry, nd);
			dput(next.dentry);
			mntput(next.mnt);
			if (err)
				goto return_err;
			err = -ENOENT;
			inode = nd->dentry->d_inode;
			if (!inode)
				break;
			err = -ENOTDIR; 
			if (!inode->i_op)
				break;
		} else {
			dput(nd->dentry);
			nd->mnt = next.mnt;
			nd->dentry = next.dentry;
		}
		err = -ENOTDIR; 
		if (!inode->i_op->lookup)
			break;
		continue;
		/* here ends the main loop */

last_with_slashes://路径的终点是个目录，如果节点是个链接，将下面两个标志位置1
		lookup_flags |= LOOKUP_FOLLOW | LOOKUP_DIRECTORY;
last_component://路径终点节点
		nd->flags &= ~LOOKUP_CONTINUE;
		if (lookup_flags & LOOKUP_PARENT)
			goto lookup_parent;
		if (this.name[0] == '.') switch (this.len) {
			default:
				break;
			case 2:	
				if (this.name[1] != '.')
					break;
				follow_dotdot(&nd->mnt, &nd->dentry);
				inode = nd->dentry->d_inode;
				/* fallthrough */
			case 1:
				goto return_reval;
		}
		if (nd->dentry->d_op && nd->dentry->d_op->d_hash) {
			err = nd->dentry->d_op->d_hash(nd->dentry, &this);
			if (err < 0)
				break;
		}
		err = do_lookup(nd, &this, &next);
		if (err)
			break;
		follow_mount(&next.mnt, &next.dentry);
		inode = next.dentry->d_inode;
		if ((lookup_flags & LOOKUP_FOLLOW)
		    && inode && inode->i_op && inode->i_op->follow_link) {
			mntget(next.mnt);
			err = do_follow_link(next.dentry, nd);
			dput(next.dentry);
			mntput(next.mnt);
			if (err)
				goto return_err;
			inode = nd->dentry->d_inode;
		} else {
			dput(nd->dentry);
			nd->mnt = next.mnt;
			nd->dentry = next.dentry;
		}
		err = -ENOENT;
		if (!inode)
			break;
		if (lookup_flags & LOOKUP_DIRECTORY) {
			err = -ENOTDIR; 
			if (!inode->i_op || !inode->i_op->lookup)
				break;
		}
		goto return_base;
lookup_parent:
		nd->last = this;
		nd->last_type = LAST_NORM;
		if (this.name[0] != '.')
			goto return_base;
		if (this.len == 1)
			nd->last_type = LAST_DOT;
		else if (this.len == 2 && this.name[1] == '.')
			nd->last_type = LAST_DOTDOT;
		else
			goto return_base;
return_reval:
		/*
		 * We bypassed the ordinary revalidation routines.
		 * We may need to check the cached dentry for staleness.
		 */
		if (nd->dentry && nd->dentry->d_sb &&
		    (nd->dentry->d_sb->s_type->fs_flags & FS_REVAL_DOT)) {
			err = -ESTALE;
			/* Note: we do not d_invalidate() */
			if (!nd->dentry->d_op->d_revalidate(nd->dentry, nd))
				break;
		}
return_base:
		return 0;
out_dput:
		dput(next.dentry);
		break;
	}
	path_release(nd);
return_err:
	return err;
}
```
该函数比较长，其中调用了几个重要的函数需要逐一进行介绍。
follow_dotdot的作用是查找当前节点的父目录。

```c

//查找当前节点的父目录
static inline void follow_dotdot(struct vfsmount **mnt, struct dentry **dentry)
{
	while(1) {
		struct vfsmount *parent;
		struct dentry *old = *dentry;

                read_lock(&current->fs->lock);
		//1.当前节点就是本进程的根节点，无法向上查找，保持不变。
		if (*dentry == current->fs->root &&
		    *mnt == current->fs->rootmnt) {
                        read_unlock(&current->fs->lock);
			break;
		}
                read_unlock(&current->fs->lock);
		spin_lock(&dcache_lock);
		//2.当前节点不是所在设备根节点，说明与父节点在同一个设备上。
		if (*dentry != (*mnt)->mnt_root) {
			//赋值为父节点
			*dentry = dget((*dentry)->d_parent);
			spin_unlock(&dcache_lock);
			//释放旧节点
			dput(old);
			break;
		}
		//3.当前节点就是所在设备的根节点，再上一级就是其他设备了
		//mnt_parent是父设备，根设备指向其自身
		parent = (*mnt)->mnt_parent;
		if (parent == *mnt) {
			//如果是根设备，结束循环，保持dentry不变
			spin_unlock(&dcache_lock);
			break;
		}
		//引用计数+1
		mntget(parent);
		//设置为安装点的上一层目录
		*dentry = dget((*mnt)->mnt_mountpoint);
		spin_unlock(&dcache_lock);
		dput(old);
		//引用计数-1
		mntput(*mnt);
		//设置当前mnt值为上层设备的vfs_mount
		*mnt = parent;
	}
	follow_mount(mnt, dentry);
}

```

do_lookup函数找到或者建立的所需的dentry结构

```c

/*
 *  It's more convoluted than I'd like it to be, but... it's still fairly
 *  small and for now I'd prefer to have fast path as straight as possible.
 *  It _is_ time-critical.
 */
static int do_lookup(struct nameidata *nd, struct qstr *name,
		     struct path *path)
{
	struct vfsmount *mnt = nd->mnt;
	//根据dentry哈希值从内存中的dentry哈希表中查找
	struct dentry *dentry = __d_lookup(nd->dentry, name);

	if (!dentry)//没找到
		goto need_lookup;
	//检查文件系统是否提供了dentry验证函数，如果验证不通过就将dentry从哈希表中脱链
	if (dentry->d_op && dentry->d_op->d_revalidate)
		goto need_revalidate;
done:
	path->mnt = mnt;
	path->dentry = dentry;
	return 0;

need_lookup:
	//磁盘中查找
	dentry = real_lookup(nd->dentry, name, nd);
	if (IS_ERR(dentry))
		goto fail;
	goto done;

need_revalidate:
	if (dentry->d_op->d_revalidate(dentry, nd))
		goto done;
	if (d_invalidate(dentry))
		goto done;
	dput(dentry);
	goto need_lookup;

fail:
	return PTR_ERR(dentry);
}


```

该函数主要是从内存中查找inode或者从磁盘查找，从内存中的哈希表中查找使用函数__d_lookup，从磁盘中查找又会调用real_lookup。

```c


//在哈希表中查找一个dentry
struct dentry * __d_lookup(struct dentry * parent, struct qstr * name)
{
	unsigned int len = name->len;
	unsigned int hash = name->hash;
	const unsigned char *str = name->name;
	//使用d_hash对父目录也计算哈希值，为了减少不同目录下的相同名称的目录，找到队列头
	struct hlist_head *head = d_hash(parent,hash);
	struct dentry *found = NULL;
	struct hlist_node *node;

	rcu_read_lock();

	
	hlist_for_each (node, head) { 
		struct dentry *dentry; 
		unsigned long move_count;
		struct qstr * qstr;

		smp_read_barrier_depends();
		dentry = hlist_entry(node, struct dentry, d_hash);

		/* if lookup ends up in a different bucket 
		 * due to concurrent rename, fail it
		 */
		if (unlikely(dentry->d_bucket != head))
			break;

		/*
		 * We must take a snapshot of d_move_count followed by
		 * read memory barrier before any search key comparison 
		 */
		move_count = dentry->d_move_count;
		smp_rmb();

		if (dentry->d_name.hash != hash)
			continue;
		if (dentry->d_parent != parent)
			continue;

		qstr = dentry->d_qstr;
		smp_read_barrier_depends();
		//使用文件系统自己定义的文件名对比函数
		if (parent->d_op && parent->d_op->d_compare) {
			if (parent->d_op->d_compare(parent, qstr, name))
				continue;
		} else {
		//使用简单的memcmp对比
			if (qstr->len != len)
				continue;
			if (memcmp(qstr->name, str, len))
				continue;
		}
		spin_lock(&dentry->d_lock);
		/*
		 * If dentry is moved, fail the lookup
		 */ 
		if (likely(move_count == dentry->d_move_count)) {
			if (!d_unhashed(dentry)) {
				atomic_inc(&dentry->d_count);
				found = dentry;
			}
		}
		spin_unlock(&dentry->d_lock);
		break;
 	}
 	rcu_read_unlock();

 	return found;
}


```


```c

/*
 * This is called when everything else fails, and we actually have
 * to go to the low-level filesystem to find out what we should do..
 *
 * We get the directory semaphore, and after getting that we also
 * make sure that nobody added the entry to the dcache in the meantime..
 * SMP-safe
 */
static struct dentry * real_lookup(struct dentry * parent, struct qstr * name, struct nameidata *nd)
{
	struct dentry * result;
	struct inode *dir = parent->d_inode;
	//从磁盘建立dentry，使用信号量，在临界区中进行。
	down(&dir->i_sem);
	/*
	 * First re-do the cached lookup just in case it was created
	 * while we waited for the directory semaphore..
	 *
	 * FIXME! This could use version numbering or similar to
	 * avoid unnecessary cache lookups.
	 *
	 * The "dcache_lock" is purely to protect the RCU list walker
	 * from concurrent renames at this point (we mustn't get false
	 * negatives from the RCU list walk here, unlike the optimistic
	 * fast walk).
	 *
	 * so doing d_lookup() (with seqlock), instead of lockfree __d_lookup
	 */
	 //down进入临界区会睡眠等待，再次在内存查找，确认没有其他进程创建了该dentry
	result = d_lookup(parent, name);
	if (!result) {
		//分配内存空间
		struct dentry * dentry = d_alloc(parent, name);
		result = ERR_PTR(-ENOMEM);

		
		if (dentry) {
			//从磁盘中寻找当前节点目录项，该函数因文件系统而异，inode节点中的i_op定义。ext2是ext2_dir_inode_operations
			result = dir->i_op->lookup(dir, dentry, nd);
			if (result)
				//撤销分配的dentry
				dput(dentry);
			else
				result = dentry;
		}
		up(&dir->i_sem);

		//成功返回dentry
		return result;
	}

```
从磁盘中寻找当前节点目录项，该函数因文件系统而异，inode节点中的i_op定义。ext2是ext2_dir_inode_operations。
继续来看ext2_lookup。

```c

/*
 * Methods themselves.
 从磁盘中加载dentry
 */

static struct dentry *ext2_lookup(struct inode * dir, struct dentry *dentry, struct nameidata *nd)
{
	struct inode * inode;
	ino_t ino;
	
	if (dentry->d_name.len > EXT2_NAME_LEN)
		return ERR_PTR(-ENAMETOOLONG);
	//找到当前节点的目录项
	ino = ext2_inode_by_name(dir, dentry);
	inode = NULL;
	if (ino) {
		//读入该目录项的索引节点，并建立inode结构
		inode = iget(dir->i_sb, ino);
		if (!inode)
			return ERR_PTR(-EACCES);
	}
	if (inode)
		return d_splice_alias(inode, dentry);
	//完成dentry结构设置，挂入哈希表中某个队列
	d_add(dentry, inode);
	return NULL;
}

```