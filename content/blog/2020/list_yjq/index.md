---
title: "链表的实现"
date: 2020-10-26T18:20:15+08:00
author: "admin0001-杨骏青转"
keywords: ["链表","实现机制"]
categories : ["新手上路"]
banner : "img/blogimg/ljrimg18.jpg"
summary : "抽象是软件设计中一项基本技术，如上所述，在众多数据结构中，选取双向链表作为基本数据结构，这就是一种提取和抽象"
---



抽象是软件设计中一项基本技术，如上所述，在众多数据结构中，选取双向链表作为基本数据结构，这就是一种提取和抽象。 1. 简约而又不简单的链表定义 于双向链表而言，内核中定义了如下简单结构： struct list_head {
struct list_head *next, *prev;
}; 这个不含任何数据项的结构，注定了它的通用性和未来使用的灵活性，例如前面的例子就可以按如下方式定义：

```
struct my_list{ 
	void *mydata; 
	struct list_head list;
	};   
```

在此，进一步说明几点： 1）list字段，隐藏了链表的指针特性，但正是它，把我们要链接的数据组织成了链表。 2）struct list_head可以位于结构的任何位置 3）可以给struct list_head起任何名字。 4）在一个结构中可以有多个list 例如，我们对要完成的任务进行描述，而任务中又包含子任务，于是有如下结构： --------------------------------------------------------------------------------------------------

\-----------------------

```
struct todo_tasks{
	char *task_name;
	unsigned int name_len;
	short int status;

	int sub_tasks;

	int subtasks_completed;
	struct list_head completed_subtasks;/* 已完成的子任务形成链表 */

	int subtasks_waiting;
	struct list_head waiting_subtasks; /* 待完成的子任务形成链表 */

	struct list_head todo_list;	/* 要完成的任务形成链表 */
	};
-----------------------------------------------------------------------
```

简约而又不简单struct list_head，以此为基本对象，就衍生了对链表的插入、删除、合并以及遍历等各种操作： 2. 链表的声明和初始化宏 实际上， struct list_head只定义了链表节点，并没有专门定义链表头，那么一个链表结构是如何建立起来的？让我们来看看下面两个宏：

```
#define LIST_HEAD_INIT(name) { &(name), &(name) }
#define LIST_HEAD(name) struct list_head name = LIST_HEAD_INIT(name)
```

如果我们要申明并定义自己的链表头mylist，直接调用LIST_HEAD： LIST_HEAD(mylist) 则mylist的next、prev指针都初始化为指向自己，这样，我们就有了一个空链表，如何判断链表是否为空，自己写一下这个简单的函数

list_empty，也就是让头指针的next指向自己而已。 3. staitic inline函数－隐藏并展开 在list.h中定义的函数大都是 staitic inline f()形式？为什么这样定义？ 关键字“static”加在函数前，表示这个函数是静态函数，所谓静态函数，实际上是对函数作用域的限制，指该函数的作用域仅 局限于本文件。所以说，static具有信息隐藏作用。 而关键字"inline“加在函数前，说明这个函数对编译程序是可见的，也就是说，编译程序在调用这个函数时就立即展开该函数。所以，关键字inline 必须与函数定义体放在一起才能使函数成为内联。inline函数一般放在头文件中。 4.  无处不在的隐藏特性 我们分析一下在链表中增加一个节点的函数实现：  ，也就是让头指针的next指向自己而已。

3. staitic inline函数－隐藏并展开
在list.h中定义的函数大都是 staitic inline f()形式？为什么这样定义？
关键字“static”加在函数前，表示这个函数是静态函数，所谓静态函数，实际上是对函数作用域的限制，指该函数的作用域仅
局限于本文件。所以说，static具有信息隐藏作用。
而关键字"inline“加在函数前，说明这个函数对编译程序是可见的，也就是说，编译程序在调用这个函数时就立即展开该函数。所以，关键字inline 必须与函数定义体放在一起才能使函数成为内联。inline函数一般放在头文件中。

4.&nbsp; 无处不在的隐藏特性
我们分析一下在链表中增加一个节点的函数实现：
有三个函数：
static inline void __list_add();
static inline void list_add();
static inline void list_add_tail();

-------------------------------------------------------------------------------------------------
/*
* Insert a new entry between two known consecutive entries.
*
* This is only for internal list manipulation where we know
* the prev/next entries already!
*/
static inline void __list_add(struct list_head *new,
struct list_head *prev,
struct list_head *next)
{
next-&gt;prev = new;
new-&gt;next = next;
new-&gt;prev = prev;
prev-&gt;next = new;
}
--------------------------------------------------------------------------------------------------
/**
* list_add - add a new entry
* @new: new entry to be added
* @head: list head to add it after
*
* Insert a new entry after the specified head.
* This is good for implementing stacks.
*/
static inline void list_add(struct list_head *new, struct list_head *head)
{
__list_add(new, head, head-&gt;next);
}
--------------------------------------------------------------------------------------------------
/**
* list_add_tail - add a new entry
* @new: new entry to be added
* @head: list head to add it before
*
* Insert a new entry before the specified head.&nbsp;&nbsp; &nbsp; &nbsp;&nbsp;&nbsp; &nbsp;&nbsp;&nbsp; &nbsp;&nbsp;&nbsp; &nbsp;&nbsp;&nbsp; &nbsp;&nbsp;&nbsp; &nbsp;&nbsp;&nbsp; &nbsp;&nbsp;&nbsp; &nbsp;&nbsp;&nbsp; &nbsp;&nbsp; 

--------------------------------------------------------------------------------------------------

仔细体会其实现代码，看起来简单有效，但实际上也是一种抽象和封装的体现。首先__list_add()函数做基本的操作，该函数仅仅是增加一个节点，至 于这个节点加到何处，暂不考虑。list_add（）调用__list_add()这个内部函数，在链表头增加一个节点，实际上实现了栈在头部增加节点的 操作，而list_add_tail()在尾部增加一个节点，实际上实现了队的操作。 至于链表的删除、搬移和合并，比较简单，不再此一一讨论 5. 链表遍历－似走过千山万水 遍历链表本是简单的，list.h中就定义了如下的宏：



--------------------------------------------------------------------------------------------------

/**
* list_for_each&nbsp;&nbsp; &nbsp;-&nbsp;&nbsp; &nbsp;iterate over a list
* @pos:&nbsp;&nbsp; &nbsp;the &amp;struct list_head to use as a loop counter.
* @head:&nbsp;&nbsp; &nbsp;the head for your list.
*/
#define list_for_each(pos, head) \
for (pos = (head)-&gt;next; pos != (head); \
pos = pos-&gt;next)



--------------------------------------------------------------------------------------------------

 这种遍历仅仅是找到一个个节点在链表中的位置pos，难点在于，如何通过pos获得节点的地址，从而可以使用节点中的数据？ 于是 list.h中定义了晦涩难懂的list_entry（）宏：

--------------------------------------------------------------------------------------------------

 

/** * list_entry - get the struct for this entry * @ptr:    the &struct list_head pointer. * @type:    the type of the struct this is embedded in. * @member:    the name of the list_struct within the struct. */ #define list_entry(ptr, type, member) \ ((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member))) 

-------------------------------------------------------------------------------------------------



为了便于理解，在此给予进一步说明。 例如

my_list结构：

```
struct my_list{ 
	void *mydata; 
	struct list_head list;
	}; 
```

struct list_head *pos;

 

则

list_entry(pos, mylist, list)宏，就可以根据pos的值，获取

mylist的地址，也就是指向

mylist的指针，这样，我们就可以存取

mylist->mydata字段了。

 

可为什么能够达到这样的效果？

 

list_entry(pos, mylist, list) 展开以后为：

 

(

(

struct my_list *

)

(

(char *)(pos) - (unsigned long)

(

&

(

(struct my_list *)0

)

->list

)

)

) 这看起来会使大多数人眩晕，但仔细分析一下，实际很简单。

 

((size_t) &(type *)0)->member)把0地址转化为type结构的指针，然后获取该结构中member成员的指针，并将其强制转换为size_t类型。于是，由于结构从0地址开始定义，因此，这样求出

member

的成员地址，实际上就是它在结构中的偏移量。为了更好的理解这些，我们可以写一段程序来验证: ---------------------------------------------------------------------------------------

 

\#include <stdio.h>

```
#include <stdlib.h>

struct foobar{
	unsigned int foo;
	char bar;
	char boo;
};

int main(int argc, char** argv){

	struct foobar tmp;

	printf("address of &tmp is= %p", &tmp);
	printf("address of tmp->foo= %p \t offset of tmp->foo= %lu\n", &tmp.foo, (unsigned long) &((struct foobar *)0)->foo);
	printf("address of tmp->bar= %p \t offset of tmp->bar= %lu\n", &tmp.bar, (unsigned long) &((struct foobar *)0)->bar);
	printf("address of tmp->boo= %p \t offset of tmp->boo= %lu", &tmp.boo, (unsigned long) &((struct foobar *)0)->boo);

	printf("computed address of &tmp using:\n");
	printf("\taddress and offset of tmp->foo= %p\n",
	(struct foobar *) (((char *) &tmp.foo) - ((unsigned long) &((struct foobar *)0)->foo)));
	printf("\taddress and offset of tmp->bar= %p\n",
	(struct foobar *) (((char *) &tmp.bar) - ((unsigned long) &((struct foobar *)0)->bar)));
	printf("\taddress and offset of tmp->boo= %p\n",
	(struct foobar *) (((char *) &tmp.boo) - ((unsigned long) &((struct foobar *)0)->boo)));

	return 0;
}
```

Output from this code is:

```
address of &tmp is= 0xbfffed00

address of tmp->foo= 0xbfffed00 offset of tmp->foo= 0
address of tmp->bar= 0xbfffed04 offset of tmp->bar= 4
address of tmp->boo= 0xbfffed05 offset of tmp->boo= 5

computed address of &tmp using:
address and offset of tmp->foo= 0xbfffed00
address and offset of tmp->bar= 0xbfffed00
address and offset of tmp->boo= 0xbfffed00
```

---------------------------------------------------------------------------------------- 到此，我们对链表的实现机制有所了解，但在此止步的话，我们依然无法领略这风景背后的韵味。 尽管list.h是内核代码中的头文件，但我们可以把它移植到用户空间使用。且看下一讲，链表API之应用

.



##### 版权声明

本文仅代表作者观点，不代表本站立场。
本文系作者授权发表，未经许可，不得转载。