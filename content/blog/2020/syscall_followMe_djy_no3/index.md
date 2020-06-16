---
title: "系统调用跟我学(3)"
date: 2020-06-15T22:32:06+08:00
author: "作者：雷镇 编辑：戴君毅"
keywords: ["系统调用"]
categories : ["系统调用"]
banner : "img/blogimg/syscall.png"
summary : "本文介绍了Linux下的进程的一些概念，并着重讲解了与Linux进程管理相关的重要系统调用wait，waitpid和exec函数族，辅助一些例程说明了它们的特点和使用方法。"
---

> 本文介绍了Linux下的进程的一些概念，并着重讲解了与Linux进程管理相关的重要系统调用wait，waitpid和exec函数族，辅助一些例程说明了它们的特点和使用方法。



## 1.7 背景

在前面的文章中，我们已经了解了父进程和子进程的概念，并已经掌握了系统调用exit的用法，但可能很少有人意识到，在一个进程调用了exit之后，该进程并非马上就消失掉，而是留下一个称为僵尸进程（Zombie）的数据结构。在Linux进程的5种状态中，僵尸进程是非常特殊的一种，它已经放弃了几乎所有内存空间，没有任何可执行代码，也不能被调度，仅仅在进程列表中保留一个位置，记载该进程的退出状态等信息供其他进程收集，除此之外，僵尸进程不再占有任何内存空间。从这点来看，僵尸进程虽然有一个很酷的名字，但它的影响力远远抵不上那些真正的僵尸兄弟，真正的僵尸总能令人感到恐怖，而僵尸进程却除了留下一些供人凭吊的信息，对系统毫无作用。

也许读者们还对这个新概念比较好奇，那就让我们来看一眼Linux里的僵尸进程究竟长什么样子。

当一个进程已退出，但其父进程还没有调用系统调用wait（稍后介绍）对其进行收集之前的这段时间里，它会一直保持僵尸状态，利用这个特点，我们来写一个简单的小程序：

```
/* zombie.c */
#include <sys/types.h>
#include <unistd.h>
main()
{
	pid_t pid;
	
	pid=fork();
	if(pid<0)  /* 如果出错 */
		printf("error occurred!\n");
	else if(pid==0) /* 如果是子进程 */
		exit(0);
	else    /* 如果是父进程 */
		sleep(60); /* 休眠60秒，这段时间里，父进程什么也干不了 */   
	wait(NULL); /* 收集僵尸进程 */
}
```

sleep的作用是让进程休眠指定的秒数，在这60秒内，子进程已经退出，而父进程正忙着睡觉，不可能对它进行收集，这样，我们就能保持子进程60秒的僵尸状态。

编译这个程序：

```
$ cc zombie.c -o zombie
```

后台运行程序，以使我们能够执行下一条命令

```
$ ./zombie &
[1] 1577
```

列一下系统内的进程

```
$ ps -ax
... ...
1177 pts/0  S   0:00 -bash
1577 pts/0  S   0:00 ./zombie
1578 pts/0  Z   0:00 [zombie <defunct>]
1579 pts/0  R   0:00 ps -ax
```

看到中间的"Z"了吗？那就是僵尸进程的标志，它表示1578号进程现在就是一个僵尸进程。

我们已经学习了系统调用exit，它的作用是使进程退出，但也仅仅限于将一个正常的进程变成一个僵尸进程，并不能将其完全销毁。僵尸进程虽然对其他进程几乎没有什么影响，不占用CPU时间，消耗的内存也几乎可以忽略不计，但有它在那里呆着，还是让人觉得心里很不舒服。而且Linux系统中进程数目是有限制的，在一些特殊的情况下，如果存在太多的僵尸进程，也会影响到新进程的产生。那么，我们该如何来消灭这些僵尸进程呢？

先来了解一下僵尸进程的来由，我们知道，Linux和UNIX总有着剪不断理还乱的亲缘关系，僵尸进程的概念也是从UNIX上继承来的，而UNIX的先驱们设计这个东西并非是因为闲来无聊想烦烦其他的程序员。僵尸进程中保存着很多对程序员和系统管理员非常重要的信息，首先，这个进程是怎么死亡的？是正常退出呢，还是出现了错误，还是被其它进程强迫退出的？其次，这个进程占用的总系统CPU时间和总用户CPU时间分别是多少？发生页错误的数目和收到信号的数目。这些信息都被存储在僵尸进程中，试想如果没有僵尸进程，进程一退出，所有与之相关的信息都立刻归于无形，而此时程序员或系统管理员需要用到，就只好干瞪眼了。

那么，我们如何收集这些信息，并终结这些僵尸进程呢？就要靠我们下面要讲到的waitpid调用和wait调用。这两者的作用都是收集僵尸进程留下的信息，同时使这个进程彻底消失。下面就对这两个调用分别作详细介绍。

## 1.8 wait

### 1.8.1 简介

wait的函数原型是：

```
 #include <``sys``/types.h> /* 提供类型pid_t的定义 */
 #include <``sys``/wait.h>
 pid_t wait(int *status)
```

进程一旦调用了wait，就立即阻塞自己，由wait自动分析是否当前进程的某个子进程已经退出，如果让它找到了这样一个已经变成僵尸的子进程，wait就会收集这个子进程的信息，并把它彻底销毁后返回；如果没有找到这样一个子进程，wait就会一直阻塞在这里，直到有一个出现为止。

参数status用来保存被收集进程退出时的一些状态，它是一个指向int类型的指针。但如果我们对这个子进程是如何死掉的毫不在意，只想把这个僵尸进程消灭掉，（事实上绝大多数情况下，我们都会这样想），我们就可以设定这个参数为NULL，就象下面这样：

```
pid = wait(NULL);
```

如果成功，wait会返回被收集的子进程的进程ID，如果调用进程没有子进程，调用就会失败，此时wait返回-1，同时errno被置为ECHILD。

### 1.8.2 实战

下面就让我们用一个例子来实战应用一下wait调用，程序中用到了系统调用fork，如果你对此不大熟悉或已经忘记了，请参考上一篇文章《进程管理相关的系统调用（一）》。

```
/* wait1.c */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdlib.h>
main()
{
	pid_t pc,pr;
	pc=fork();
	if(pc<0)     /* 如果出错 */
		printf("error ocurred!\n");
	else if(pc==0){   /* 如果是子进程 */ 
		printf("This is child process with pid of %d\n",getpid());
		sleep(10); /* 睡眠10秒钟 */  
	}
	else{      /* 如果是父进程 */
		pr=wait(NULL); /* 在这里等待 */
		printf("I catched a child process with pid of %d\n"),pr);
	}    
	exit(0);
}
```

编译并运行:

```
$ cc wait1.c -o wait1
$ ./wait1
This is child process with pid of 1508
I catched a child process with pid of 1508
```

可以明显注意到，在第2行结果打印出来前有10秒钟的等待时间，这就是我们设定的让子进程睡眠的时间，只有子进程从睡眠中苏醒过来，它才能正常退出，也就才能被父进程捕捉到。其实这里我们不管设定子进程睡眠的时间有多长，父进程都会一直等待下去，读者如果有兴趣的话，可以试着自己修改一下这个数值，看看会出现怎样的结果。

### 1.8.3 参数status

如果参数status的值不是NULL，wait就会把子进程退出时的状态取出并存入其中，这是一个整数值（int），指出了子进程是正常退出还是被非正常结束的（一个进程也可以被其他进程用信号结束，我们将在以后的文章中介绍），以及正常结束时的返回值，或被哪一个信号结束的等信息。由于这些信息被存放在一个整数的不同二进制位中，所以用常规的方法读取会非常麻烦，人们就设计了一套专门的宏（macro）来完成这项工作，下面我们来学习一下其中最常用的两个：

1，WIFEXITED(status) 这个宏用来指出子进程是否为正常退出的，如果是，它会返回一个非零值。

（请注意，虽然名字一样，这里的参数status并不同于wait唯一的参数--指向整数的指针status，而是那个指针所指向的整数，切记不要搞混了。）

2，WEXITSTATUS(status) 当WIFEXITED返回非零值时，我们可以用这个宏来提取子进程的返回值，如果子进程调用exit(5)退出，WEXITSTATUS(status)就会返回5；如果子进程调用exit(7)，WEXITSTATUS(status)就会返回7。请注意，如果进程不是正常退出的，也就是说，WIFEXITED返回0，这个值就毫无意义。

下面通过例子来实战一下我们刚刚学到的内容：

```
/* wait2.c */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
main()
{
	int status;
	pid_t pc,pr;
	pc=fork();
	if(pc<0) /* 如果出错 */
		printf("error ocurred!\n");
	else if(pc==0){ /* 子进程 */
		printf("This is child process with pid of %d.\n",getpid());  			exit(3);  /* 子进程返回3 */
	}
	else{    /* 父进程 */
		pr=wait(&status);
		if(WIFEXITED(status)){ /* 如果WIFEXITED返回非零值 */ 
			printf("the child process %d exit normally.\n",pr);   					printf("the return code is %d.\n",WEXITSTATUS(status));  
		}else      /* 如果WIFEXITED返回零 */
			printf("the child process %d exit abnormally.\n",pr); 
	}
}
```

编译并运行:

```
$ cc wait2.c -o wait2
$ ./wait2``This is child process with pid of 1538.
the child process 1538 exit normally.
the return code is 3.
```

父进程准确捕捉到了子进程的返回值3，并把它打印了出来。

当然，处理进程退出状态的宏并不止这两个，但它们当中的绝大部分在平时的编程中很少用到，就也不在这里浪费篇幅介绍了，有兴趣的读者可以自己参阅Linux man pages去了解它们的用法。

### 1.8.4 进程同步

有时候，父进程要求子进程的运算结果进行下一步的运算，或者子进程的功能是为父进程提供了下一步执行的先决条件（如：子进程建立文件，而父进程写入数据），此时父进程就必须在某一个位置停下来，等待子进程运行结束，而如果父进程不等待而直接执行下去的话，可以想见，会出现极大的混乱。这种情况称为进程之间的同步，更准确地说，这是进程同步的一种特例。进程同步就是要协调好2个以上的进程，使之以安排好地次序依次执行。解决进程同步问题有更通用的方法，我们将在以后介绍，但对于我们假设的这种情况，则完全可以用wait系统调用简单的予以解决。请看下面这段程序：

```
#include <sys/types.h>
#include <sys/wait.h>
main()
{
	pid_t pc, pr;
	int status;
	
	pc=fork();
	
	if(pc<0)
		printf("Error occured on forking.\n");
	else if(pc==0){
		/* 子进程的工作 */
		exit(0);
	}else{
		/* 父进程的工作 */
		pr=wait(&status);
		/* 利用子进程的结果 */
	}
}
```

这段程序只是个例子，不能真正拿来执行，但它却说明了一些问题，首先，当fork调用成功后，父子进程各做各的事情，但当父进程的工作告一段落，需要用到子进程的结果时，它就停下来调用wait，一直等到子进程运行结束，然后利用子进程的结果继续执行，这样就圆满地解决了我们提出的进程同步问题。

## 1.9 waitpid

### 1.9.1 简介

waitpid系统调用在Linux函数库中的原型是：

```
  #include <sys/types.h> /* 提供类型pid_t的定义 */
#include <sys/wait.h>
pid_t waitpid(pid_t pid,int *status,int options)
```

从本质上讲，系统调用waitpid和wait的作用是完全相同的，但waitpid多出了两个可由用户控制的参数pid和options，从而为我们编程提供了另一种更灵活的方式。下面我们就来详细介绍一下这两个参数：

### pid

从参数的名字pid和类型pid_t中就可以看出，这里需要的是一个进程ID。但当pid取不同的值时，在这里有不同的意义。

1. pid>0时，只等待进程ID等于pid的子进程，不管其它已经有多少子进程运行结束退出了，只要指定的子进程还没有结束，waitpid就会一直等下去。
2. pid=-1时，等待任何一个子进程退出，没有任何限制，此时waitpid和wait的作用一模一样。
3. pid=0时，等待同一个进程组中的任何子进程，如果子进程已经加入了别的进程组，waitpid不会对它做任何理睬。
4. pid<-1时，等待一个指定进程组中的任何子进程，这个进程组的ID等于pid的绝对值。

### options

options提供了一些额外的选项来控制waitpid，目前在Linux中只支持WNOHANG和WUNTRACED两个选项，这是两个常数，可以用"|"运算符把它们连接起来使用，比如：

```
ret=waitpid(-1,NULL,WNOHANG | WUNTRACED);
```

如果我们不想使用它们，也可以把options设为0，如：

```
ret=waitpid(-1,NULL,0);
```

如果使用了WNOHANG参数调用waitpid，即使没有子进程退出，它也会立即返回，不会像wait那样永远等下去。

而WUNTRACED参数，由于涉及到一些跟踪调试方面的知识，加之极少用到，这里就不多费笔墨了，有兴趣的读者可以自行查阅相关材料。

看到这里，聪明的读者可能已经看出端倪了--wait不就是经过包装的waitpid吗？没错，察看<内核源码目录>/include/unistd.h文件349-352行就会发现以下程序段：

```
static inline pid_t wait(int * wait_stat)
{
	return waitpid(-1,wait_stat,0);
}
```

### 1.9.2 返回值和错误

waitpid的返回值比wait稍微复杂一些，一共有3种情况：

1. 当正常返回的时候，waitpid返回收集到的子进程的进程ID；
2. 如果设置了选项WNOHANG，而调用中waitpid发现没有已退出的子进程可收集，则返回0；
3. 如果调用中出错，则返回-1，这时errno会被设置成相应的值以指示错误所在；

当pid所指示的子进程不存在，或此进程存在，但不是调用进程的子进程，waitpid就会出错返回，这时errno被设置为ECHILD；

```
/* waitpid.c */
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
main()
{
	pid_t pc, pr;
	
	pc=fork();
	if(pc<0)   /* 如果fork出错 */
		printf("Error occured on forking.\n");
	else if(pc==0){   /* 如果是子进程 */
		sleep(10); /* 睡眠10秒 */
		exit(0);
	}
	/* 如果是父进程 */
	do{
		pr=waitpid(pc, NULL, WNOHANG); /* 使用了WNOHANG参数，waitpid不会在这里等待 */
		if(pr==0){     /* 如果没有收集到子进程 */
			printf("No child exited\n");
			sleep(1);
		}
	}while(pr==0);       /* 没有收集到子进程，就回去继续尝试 */
	if(pr==pc)
		printf("successfully get child %d\n", pr);
	else
		printf("some error occured\n");
}
```

编译并运行：

```
$ cc waitpid.c -o waitpid
$ ./waitpid
No child exited
No child exited
No child exited
No child exited
No child exited
No child exited
No child exited
No child exited
No child exited
No child exited
successfully get child 1526
```

父进程经过10次失败的尝试之后，终于收集到了退出的子进程。

因为这只是一个例子程序，不便写得太复杂，所以我们就让父进程和子进程分别睡眠了10秒钟和1秒钟，代表它们分别作了10秒钟和1秒钟的工作。父子进程都有工作要做，父进程利用工作的简短间歇察看子进程的是否退出，如退出就收集它。

## 1.10 exec

也许有不少读者从本系列文章一推出就开始读，一直到这里还有一个很大的疑惑：既然所有新进程都是由fork产生的，而且由fork产生的子进程和父进程几乎完全一样，那岂不是意味着系统中所有的进程都应该一模一样了吗？而且，就我们的常识来说，当我们执行一个程序的时候，新产生的进程的内容应就是程序的内容才对。是我们理解错了吗？显然不是，要解决这些疑惑，就必须提到我们下面要介绍的exec系统调用。

### 1.10.1 简介

说是exec系统调用，实际上在Linux中，并不存在一个exec()的函数形式，exec指的是一组函数，一共有6个，分别是：

```
#include <unistd.h>
int execl(const char *path, const char *arg, ...);
int execlp(const char *file, const char *arg, ...);
int execle(const char *path, const char *arg, ..., char *const envp[]);
int execv(const char *path, char *const argv[]);
int execvp(const char *file, char *const argv[]);
int execve(const char *path, char *const argv[], char *const envp[]);
```

其中只有execve是真正意义上的系统调用，其它都是在此基础上经过包装的库函数。

exec函数族的作用是根据指定的文件名找到可执行文件，并用它来取代调用进程的内容，换句话说，就是在调用进程内部执行一个可执行文件。这里的可执行文件既可以是二进制文件，也可以是任何Linux下可执行的脚本文件。

与一般情况不同，exec函数族的函数执行成功后不会返回，因为调用进程的实体，包括代码段，数据段和堆栈等都已经被新的内容取代，只留下进程ID等一些表面上的信息仍保持原样，颇有些神似"三十六计"中的"金蝉脱壳"。看上去还是旧的躯壳，却已经注入了新的灵魂。只有调用失败了，它们才会返回一个-1，从原程序的调用点接着往下执行。

现在我们应该明白了，Linux下是如何执行新程序的，每当有进程认为自己不能为系统和拥护做出任何贡献了，他就可以发挥最后一点余热，调用任何一个exec，让自己以新的面貌重生；或者，更普遍的情况是，如果一个进程想执行另一个程序，它就可以fork出一个新进程，然后调用任何一个exec，这样看起来就好像通过执行应用程序而产生了一个新进程一样。

事实上第二种情况被应用得如此普遍，以至于Linux专门为其作了优化，我们已经知道，fork会将调用进程的所有内容原封不动的拷贝到新产生的子进程中去，这些拷贝的动作很消耗时间，而如果fork完之后我们马上就调用exec，这些辛辛苦苦拷贝来的东西又会被立刻抹掉，这看起来非常不划算，于是人们设计了一种"写时拷贝（copy-on-write）"技术，使得fork结束后并不立刻复制父进程的内容，而是到了真正实用的时候才复制，这样如果下一条语句是exec，它就不会白白作无用功了，也就提高了效率。

### 1.10.2 稍稍深入

上面6条函数看起来似乎很复杂，但实际上无论是作用还是用法都非常相似，只有很微小的差别。在学习它们之前，先来了解一下我们习以为常的main函数。

下面这个main函数的形式可能有些出乎我们的意料：

```
int main(int argc, char *argv[], char *envp[])
```

它可能与绝大多数教科书上描述的都不一样，但实际上，这才是main函数真正完整的形式。

参数argc指出了运行该程序时命令行参数的个数，数组argv存放了所有的命令行参数，数组envp存放了所有的环境变量。环境变量指的是一组值，从用户登录后就一直存在，很多应用程序需要依靠它来确定系统的一些细节，我们最常见的环境变量是PATH，它指出了应到哪里去搜索应用程序，如/bin；HOME也是比较常见的环境变量，它指出了我们在系统中的个人目录。环境变量一般以字符串"XXX=xxx"的形式存在，XXX表示变量名，xxx表示变量的值。

值得一提的是，argv数组和envp数组存放的都是指向字符串的指针，这两个数组都以一个NULL元素表示数组的结尾。

我们可以通过以下这个程序来观看传到argc、argv和envp里的都是什么东西：

```
/* main.c */
int main(int argc, char *argv[], char *envp[])
{
	printf("\n### ARGC ###\n%d\n", argc);
	printf("\n### ARGV ###\n");
	while(*argv)
		printf("%s\n", *(argv++));
	printf("\n### ENVP ###\n");
	while(*envp)
		printf("%s\n", *(envp++));
	return 0;
}
```

编译它：

```
$ cc main.c -o main
```

运行时，我们故意加几个没有任何作用的命令行参数：

```
$ ./main -xx 000
### ARGC ###
3
### ARGV ###
./main
-xx
000
### ENVP ###
PWD=/home/lei
REMOTEHOST=dt.laser.com
HOSTNAME=localhost.localdomain
QTDIR=/usr/lib/qt-2.3.1
LESSOPEN=|/usr/bin/lesspipe.sh%s
KDEDIR=/usr
USER=lei
LS_COLORS=
MACHTYPE=i386-redhat-linux-gnu
MAIL=/var/spool/mail/lei
INPUTRC=/etc/inputrc
LANG=en_US
LOGNAME=lei
SHLVL=1
SHELL=/bin/bash
HOSTTYPE=i386
OSTYPE=linux-gnu
HISTSIZE=1000
TERM=ansi
HOME=/home/lei
PATH=/usr/local/bin:/bin:/usr/bin:/usr/X11R6/bin:/home/lei/bin
_=./main
```

我们看到，程序将"./main"作为第1个命令行参数，所以我们一共有3个命令行参数。这可能与大家平时习惯的说法有些不同，小心不要搞错了。

现在回过头来看一下exec函数族，先把注意力集中在execve上：

```
int execve(const char *path, char *const argv[], char *const envp[]);
```

对比一下main函数的完整形式，看出问题了吗？是的，这两个函数里的argv和envp是完全一一对应的关系。execve第1个参数path是被执行应用程序的完整路径，第2个参数argv就是传给被执行应用程序的命令行参数，第3个参数envp是传给被执行应用程序的环境变量。

留心看一下这6个函数还可以发现，前3个函数都是以execl开头的，后3个都是以execv开头的，它们的区别在于，execv开头的函数是以"char *argv[]"这样的形式传递命令行参数，而execl开头的函数采用了我们更容易习惯的方式，把参数一个一个列出来，然后以一个NULL表示结束。这里的NULL的作用和argv数组里的NULL作用是一样的。

在全部6个函数中，只有execle和execve使用了char *envp[]传递环境变量，其它的4个函数都没有这个参数，这并不意味着它们不传递环境变量，这4个函数将把默认的环境变量不做任何修改地传给被执行的应用程序。而execle和execve会用指定的环境变量去替代默认的那些。

还有2个以p结尾的函数execlp和execvp，咋看起来，它们和execl与execv的差别很小，事实也确是如此，除execlp和execvp之外的4个函数都要求，它们的第1个参数path必须是一个完整的路径，如"/bin/ls"；而execlp和execvp的第1个参数file可以简单到仅仅是一个文件名，如"ls"，这两个函数可以自动到环境变量PATH制定的目录里去寻找。

### 1.10.3 实战

知识介绍得差不多了，接下来我们看看实际的应用：

```
/* exec.c */
#include <unistd.h>
main()
{
	char *envp[]={"PATH=/tmp",
		"USER=lei",
		"STATUS=testing",
		NULL};
	char *argv_execv[]={"echo", "excuted by execv", NULL};
	char *argv_execvp[]={"echo", "executed by execvp", NULL};
	char *argv_execve[]={"env", NULL};
	if(fork()==0)
		if(execl("/bin/echo", "echo", "executed by execl", NULL)<0)
			perror("Err on execl");
	if(fork()==0)
		if(execlp("echo", "echo", "executed by execlp", NULL)<0)    
			perror("Err on execlp");
	if(fork()==0)
		if(execle("/usr/bin/env", "env", NULL, envp)<0)
			error("Err on execle");
	if(fork()==0)
		if(execv("/bin/echo", argv_execv)<0)
			perror("Err on execv");
	if(fork()==0)
		if(execvp("echo", argv_execvp)<0)
			perror("Err on execvp");
	if(fork()==0)
		if(execve("/usr/bin/env", argv_execve, envp)<0)
			perror("Err on execve");
}
```

程序里调用了2个Linux常用的系统命令，echo和env。echo会把后面跟的命令行参数原封不动的打印出来，env用来列出所有环境变量。

由于各个子进程执行的顺序无法控制，所以有可能出现一个比较混乱的输出--各子进程打印的结果交杂在一起，而不是严格按照程序中列出的次序。

编译并运行：

```
$ cc exec.c -o exec
$ ./exec
executed by execl
PATH=/tmp
USER=lei
STATUS=testing
executed by execlp
excuted by execv
executed by execvp
PATH=/tmp
USER=lei
STATUS=testing
```

果然不出所料，execle输出的结果跑到了execlp前面。

大家在平时的编程中，如果用到了exec函数族，一定记得要加错误判断语句。因为与其他系统调用比起来，exec很容易受伤，被执行文件的位置，权限等很多因素都能导致该调用的失败。最常见的错误是：

1. 找不到文件或路径，此时errno被设置为ENOENT；
2. 数组argv和envp忘记用NULL结束，此时errno被设置为EFAULT；
3. 没有对要执行文件的运行权限，此时errno被设置为EACCES。

## 1.11 进程的一生

下面就让我用一些形象的比喻，来对进程短暂的一生作一个小小的总结：

随着一句fork，一个新进程呱呱落地，但它这时只是老进程的一个克隆。

然后随着exec，新进程脱胎换骨，离家独立，开始了为人民服务的职业生涯。

人有生老病死，进程也一样，它可以是自然死亡，即运行到main函数的最后一个"}"，从容地离我们而去；也可以是自杀，自杀有2种方式，一种是调用exit函数，一种是在main函数内使用return，无论哪一种方式，它都可以留下遗书，放在返回值里保留下来；它还甚至能可被谋杀，被其它进程通过另外一些方式结束他的生命。

进程死掉以后，会留下一具僵尸，wait和waitpid充当了殓尸工，把僵尸推去火化，使其最终归于无形。

这就是进程完整的一生。

## 1.12 小结

本文重点介绍了系统调用wait、waitpid和exec函数族，对与进程管理相关的系统调用的介绍就在这里告一段落，在下一篇文章，也是与进程管理相关的系统调用的最后一篇文章中，我们会通过两个很酷的实际例子，来重温一下最近学过的知识。

#### 相关主题

- Linux man pages
- Advanced Programming in the UNIX Environment by W. Richard Stevens, 1993
- Linux核心源代码分析 彭晓明，王强，2000

---

##### 版权声明

本文仅代表作者观点，不代表本站立场。
本文系作者授权发表，未经许可，不得转载。

