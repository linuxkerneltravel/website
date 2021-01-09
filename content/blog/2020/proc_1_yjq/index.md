---
title: "proc文件系统探索 之 以数字命名的目录[一]"
date: 2020-12-19T10:21:15+08:00
author: "杨骏青转"
keywords: ["proc"]
categories : ["走进内核"]
banner : "img/blogimg/cxl_3.jpg"
summary : "在proc根目录下，以数字命名的目录表示当前一个运行的进程，目录名即为进程的pid。其内的目录和文件给出了一些关于该进程的信息。"
---

在proc根目录下，以数字命名的目录表示当前一个运行的进程，目录名即为进程的pid。其内的目录和文件给出了一些关于该进程的信息。

```
niutao@niutao-desktop:/proc/6584$ ls attr        coredump_filter  fd        maps        oom_score  statm auxv        cpuset           fdinfo    mem         root       status cgroup      cwd              io        mounts      sched      task clear_refs  environ          limits    mountstats  smaps      wchan cmdline     exe              loginuid  oom_adj     stat
```

我们可以看到该目录下有这么些文件。其中attr、fd、fdinfo、task为目录，cwd、root为指向目录的链接，exe为指向文件的链接，其余为一般的文件。对于一些文件或目录操纵的权限(查看或者修改的权限)是该进程的创建者才有，例如auxv、envion、fd、fdinfo、 limits、mem、mountstats等文件或目录只有创建该进程的用户才具有查看或者进入的权限，而其他一些文件则对所有用户具有可读权限。关于这些文件或目录的权限，我们可以在内核中找到(fs/proc/base.c tid_base_stuff数组)。下我们来详细探讨每一个文件或目录的作用. 1．cmdline文件： 该文件中包含的是该进程的命令行参数，包括进程的启动路径(argv[0])。也就是说例如你在命令行上运行一个hello程序:

```
niutao@niutao-desktop:~$ cat hello.c #include<stdio.h> #include<wait.h> int main() { int i; for(i=0;i<100;i++) { printf("Hello world\n"); sleep(2); } return 0; } niutao@niutao-desktop:~$ gcc -o hello hello.c niutao@niutao-desktop:~$ ./hello one two niutao@niutao-desktop:~$ ps -A |grep hello 7282 pts/4 00:00:00 hello niutao@niutao-desktop:~$ cd /proc/7282/ niutao@niutao-desktop:/proc/7282$ cat cmdline ./helloonetwoniutao@niutao-desktop:/proc/7282$
```

可以看到cmdline里的内容为"./helloonetwo"，正是命令行的参数。可能你会疑问为什幺参数没有分开？呵呵，那是因为cat欺骗了你。我们可以做一个实验，将该cmdline文件复制到你的用户目录下，如果使用vim查看就会发现是这样: ./hello^@one^@two^@ 也就是说实际每个参数之间是有东西隔开的，之不过cat将其忽略了而已，而vim可以给你标识出有东西，但vim本身不可显示罢了。我们可以通过编程读取该文件。下面给出我写的一个读取该文件的小程序。 我们一个字符一个字符的读取文件内容直到文件结束，在读取没一个字符的时候，打印其字符和对应的数值：

```
niutao@niutao-desktop:~/c$ cat readcmd.c #include<stdio.h> #include<string.h> #include <sys/types.h> #include <sys/stat.h> #include <fcntl.h> int main(int args,char *argv[]) { FILE *fp; char path[80]; unsigned char ch; snprintf(path,80,"/home/niutao/cmdline"); if((fp=fopen(path,"r"))==NULL) { perror("fopen"); return 0; } while(!feof(fp)) { ch=fgetc(fp); printf("%c %d\n",ch,ch); } fclose(fp); return 0; } niutao@niutao-desktop:~/c$ gcc -o readcmd readcmd.c niutao@niutao-desktop:~/c$ ./readcmd . 46 / 47 h 104 e 101 l 108 l 108 o 111 0 o 111 n 110 e 101 0 t 116 w 119 o 111 0 � 255``niutao@niutao-desktop:~/c$
```

由此我们可以看出并非是每个参数之间没有间隔，而已以字符'\0'作间隔。所以如果我们在某一程序中想读取某个进程的命令行参数，我们只需要知道该进程的pid，然后进入proc文件系统的该pid对应的目录下，编程读取cmdline文件就可以了。

.