---
title: "Makefile_write_yjq"
date: 2020-10-13T22:14:58+08:00
author: "杨骏青"
keywords: ["Makefile"]
categories : ["新手上路"]
banner : "img/blogimg/ljrimg20.jpg"
summary : "本篇文章主要讲了Makefile的一些优点、必要性、以及编写的时候的需要遵守的一些规则，可以帮助我们更好的掌握Makefile的编写"
---

# Makefile 编写(上)

## 一、为什么需要Makefile

makefile关系到了整个工程的编译规则。一个工程中的源文件不计数，其按类型、功能、模块分别放在若干个目录中，makefile定义了一系列的规则来指定，哪些文件需要先编译，哪些文件需要后编译，哪些文件需要重新编译，甚至于进行更复杂的功能操作，因为makefile就像一个Shell脚本一样，其中也可以执行操作系统的命令。

## 二、Makefile的好处

makefile带来的好处就是——“自动化编译”，一旦写好，只需要一个make命令，整个工程完全自动编译，极大的提高了软件开发的效率。

make是一个命令工具，是一个解释makefile中指令的命令工具，一般来说，大多数的IDE都有这个命令，比如：Delphi的make，Visual C++的nmake，Linux下GNU的make。可见，makefile都成为了一种在工程方面的编译方法。

**当然，不同产商的make各不相同，也有不同的语法，但其本质都是在“文件依赖性”上做文章**

## 三、Makefile的基本规则

### 1.基本规则

**在Makefile中，规则的顺序是很重要的，因为，Makefile中只应该有一个最终目标，其它的目标都是被这个目标所连带出来的，所以一定要让make知道你的最终目标是什么。**

一般来说，定义在Makefile中的目标可能会有很多，但是第一条规则中的目标将被确立为最终的目标。

如果第一条规则中的目标有很多个，那么，第一个目标会成为最终的目标。make所完成的也就是这个目标



make支持三各通配符：“*”，“?”和“[...]”

波浪号（“~”）字符在文件名中也有比较特殊的用途。

如果是“~/test”，这就表示当前用户的$HOME目录下的test目录。

“~hchen/test”则表示用户hchen的宿主目录下的test目录

### 2.代码规则

```
target ... : prerequisites ...
            command
            ...
            ...
```

* target也就是一个目标文件，可以是Object File，也可以是执行文件。

  还可以是一个标签（Label），对于标签这种特性，在后续的“伪目标”章节中会有叙述。

* prerequisites就是，要生成那个target所需要的文件或是目标。

* command也就是make需要执行的命令。（任意的Shell命令）

**prerequisites中如果有一个以上的文件比target文件要新的话，command所定义的命令就会被执行。这就是Makefile的规则。**

这里要说明一点的是，clean不是一个文件，它只不过是一个动作名字，其冒号后什么也没有，那么，make就不会自动去找文件的依赖性，也就不会自动执行其后所定义的命令。要执行其后的命令，就要在make命令后明显得指出这个“标签”的名字。这样的方法非常有用，我们可以在一个makefile中定义不用的编译或是和编译无关的命令，比如程序的打包，程序的备份，等等。

## 五、让make自动推导

GNU的make很强大，它可以自动推导文件以及文件依赖关系后面的命令，于是我们就没必要去在每一个[.o]文件后都写上类似的命令，因为，我们的make会自动识别，并自己推导命令。

只要make看到一个[.o]文件，它就会自动的把[.c]文件加在依赖关系中，如果make找到一个whatever.o，那么whatever.c，就会是whatever.o的依赖文件。并且 cc -c whatever.c 也会被推导出来，于是，我们的makefile再也不用写得这么复杂。我们的是新的makefile又出炉了



## 四、清空目标文件规则

```makefile
一般的风格都是：

        clean:
            rm edit $(objects)

更为稳健的做法是：

        .PHONY : clean
        clean :
                -rm edit $(objects)
```

“.PHONY”表示，clean是个**伪目标文件**

而在rm命令前面加了一个小减号的意思就是，也许某些文件出现问题，但不要管，继续做后面的事。当然，clean的规则不要放在文件的开头，不然，这就会变成make的默认目标，相信谁也不愿意这样。不成文的规矩是——“clean从来都是放在文件的最后”。

## 五、伪目标

```makefile
clean:
            rm *.o temp
```



因为，我们并不生成“clean”这个文件。“伪目标”并不是一个文件，只是一个标签，由于“伪目标”不是文件，所以make无法生成它的依赖关系和决定它是否要执行。我们只有通过显示地指明这个“目标”才能让其生效。当然，“伪目标”的取名不能和文件名重名，不然其就失去了“伪目标”的意义了。

`.PHONY : clean`

当然，为了避免和文件重名的这种情况，我们可以使用一个特殊的标记“.PHONY”来显示地指明一个目标是“伪目标”，向make说明，不管是否有这个文件，这个目标就是“伪目标”。

## 六、变量

- 变量的命名字可以包含字符、数字，下划线（可以是数字开头）

- 但不应该含有“:”、“#”、“=”或是空字符（空格、回车等）

- 变量是大小写敏感的，“foo”、“Foo”和“FOO”是三个不同的变量名

- 传统的Makefile的变量名是全大写的命名方式

### 1.变量赋值

变量在声明时需要给予初值，而在使用时，需要给在变量名前加上“$”符号，用小括号“（）”或是大括号“{}”把变量给包括起来

```makefile
    objects = program.o foo.o utils.o
    program : $(objects)
            cc -o program $(objects)

    $(objects) : defs.h
```

### 2.变量中的变量

#### 2.1 “=”

```makefile
    foo = $(bar)
    bar = $(ugh)
    ugh = test!!!

    all:
            echo $(foo)
```

最后打印出来应该就是 test！！！

- 特点：前面的变量可以使用后面的变量

```makefile
    CFLAGS = $(include_dirs) -O
    include_dirs = -Ifoo -Ibar
```

  当“CFLAGS”在命令中被展开时，会是“-Ifoo -Ibar -o”

#### 2.2 “:=”

- 特点：前面的变量不能使用后面的变量，只能使用前面已定义好了的变量

```makefile
    x := A
    y := $(x) B
    x := c
```

最后y的值应该是 AB ,x的值是 C



#### 2.3 "?="

```
A ?= aaa
```

- 含义:

  如果A没有被定义过，那么变量A的值就是“aaa”，如果A先前被定义过，那么这条语将什么也不做



#### 2.4 "+="

```makefile
    objects = main.o foo.o bar.o utils.o
    objects += another.o
```

- 含义：

  在使用了以后，我们的$(objects)值变成：“main.o foo.o bar.o utils.o another.o”（another.o被追加进去了）







## 七、条件分支语句

```makefile
libs_for_gcc = -lgnu
normal_libs =
 
foo: $(objects)
ifeq ($(CC),gcc)
$(CC) -o foo $(objects) $(libs_for_gcc)
else
$(CC) -o foo $(objects) $(normal_libs)
endif


当我们的变量$(CC)值是“gcc”时，目标foo的规则是：

foo: $(objects)

$(CC) -o foo $(objects) $(libs_for_gcc)

而当我们的变量$(CC)值不是“gcc”时（比如“cc”），目标foo的规则是：

foo: $(objects)

$(CC) -o foo $(objects) $(normal_libs)
```

**类似于ifeq的关键字有4个**

* ifeq - 比较是否相同，如果相同，则为真

ifeq (<arg1>, <arg2> )

ifeq '<arg1>' '<arg2>'

ifeq "<arg1>" "<arg2>"

ifeq "<arg1>" '<arg2>'

ifeq '<arg1>' "<arg2>"

* ifneq  - 比较是否相同，如果不同，则为真

ifneq (<arg1>, <arg2> )

ifneq '<arg1>' '<arg2>'

ifneq "<arg1>" "<arg2>"

ifneq "<arg1>" '<arg2>'

ifneq '<arg1>' "<arg2>"

* ifdef- 如果变量<variable-name>的值非空，那到表达式为真。否则，表达式为假。

ifdef <variable-name>

* ifndef

和上面相反

在首个关键字这行，多余的空格是被允许的，但是不能以[Tab]键做为开始（不然就被认为是命令）。而注释符“#”同样也是安全的。“else”和“endif”也一样，只要不是以[Tab]键开始就行了。

## 八、函数调用语句

### 1.函数调用语法 

函数调用，很像变量的使用，也是以“$”来标识的，其语法如下： 

  $(<function> <arguments> )

  或是

  ${<function> <arguments>}

```makefile
comma:= ,
empty:=
space:= $(empty) $(empty)
foo:= a b c
bar:= $(subst $(space),$(comma),$(foo))
```

$(comma)的值是一个逗号。$(space)使用了$(empty)定义了一个空格，$(foo)的值是“a b c”

“subst”，这是一个替换函数，这个函数有三个参数，第一个参数是被替换字串，第二个参数是替换字串，第三个参数是替换操作作用的字串。这个函数也就是把$(foo)中的空格替换成逗号，所以$(bar)的值是“a,b,c”

### 2.常用函数

#### 2.1字符串处理函数


| $(subst <from>,<to>,<text> )                | 字符串替换       |
| ------------------------------------------- | ---------------- |
| $(patsubst <pattern>,<replacement>,<text> ) | 模式字符串替换   |
| $(strip <string> )                          | 去空格           |
| $(findstring <find>,<in> )                  | 查找字符串       |
| $(filter <pattern...>,<text> )              | 过滤函数         |
| $(filter-out <pattern...>,<text> )          | 反过滤函数       |
| $(sort <list> )                             | 排序函数         |
| $(word <n>,<text> )                         | 取单词函数       |
| $(wordlist <s>,<e>,<text> )                 | 取单词串函数     |
| $(words <text> )                            | 单词个数统计函数 |
| $(firstword <text> )                        | 取首单词函数     |

#### 2.2文件名操作函数

| $(dir <names...> )                | 取目录函数 |
| --------------------------------- | ---------- |
| $(notdir <names...> )             | 取文件函数 |
| $(suffix <names...> )             | 取后缀函数 |
| $(basename <names...> )           | 取前缀函数 |
| $(addsuffix <suffix>,<names...> ) | 加后缀函数 |
| $(addprefix <prefix>,<names...> ) | 加前缀函数 |
| $(join <list1>,<list2> )          | 连接函数   |

#### 2.3其他函数

| $(foreach <var>,<list>,<text> )                 | 循环函数                           |
| ----------------------------------------------- | ---------------------------------- |
| $(if <condition>,<then-part> )                  | 条件语句函数                       |
| $(call <expression>,<parm1>,<parm2>,<parm3>...) | 创建新的参数化的函数               |
| $(origin <variable> )                           | 返回值来告诉你这个变量的“出生情况” |
| contents := $(shell cat foo)                    | 执行shell命令                      |
| $(error <text ...> )                            | 控制make运行的函数                 |
|                                                 |                                    |
|                                                 |                                    |





## 十二、显示命令

* `@echo 正在编译XXX模块......`

当make执行时，会输出“正在编译XXX模块......”字串，但不会输出命令

如果没有“@”，那么，make将输出：

 ```
echo 正在编译XXX模块......
     正在编译XXX模块......
 ```



* PS:

  如果make执行时，带入make参数“-n”或“--just-print”，那么其只是显示命令，但不会执行命令，这个功能很有利于我们调试我们的Makefile，看看我们书写的命令是执行起来是什么样子的或是什么顺序的。





## 十三、执行命令

* 需要注意的是，如果你要让上一条命令的结果应用在下一条命令时，你应该使用分号分隔这两条命令。比如你的第一条命令是cd命令，你希望第二条命令得在cd之后的基础上运行，那么你就不能把这两条命令写在两行上，而应该把这两条命令写在一行上，用分号分隔。如：

     ```makefile
   示例一：
        exec:
                cd /home/hchen
                pwd

    示例二：
        exec:
                cd /home/hchen; pwd
   ```
   
   

当我们执行“make exec”时，第一个例子中的cd没有作用，pwd会打印出当前的Makefile目录，而第二个例子中，cd就起作用了，pwd会打印出“/home/hchen”。



## 十四、忽视命令出错

每当命令运行完后，make会检测每个命令的返回码，如果命令返回成功，那么make会执行下一条命令，当规则中所有的命令成功返回后，这个规则就算是成功完成了。如果一个规则中的某个命令出错了（命令退出码非零），那么make就会终止执行当前规则，这将有可能终止所有规则的执行。



有些时候，命令的出错并不表示就是错误的。例如mkdir命令，我们一定需要建立一个目录，如果目录不存在，那么mkdir就成功执行，万事大吉，如果目录存在，那么就出错了。我们之所以使用mkdir的意思就是一定要有这样的一个目录，于是我们就不希望mkdir出错而终止规则的运行。



为了做到这一点，忽略命令的出错，我们可以在Makefile的命令行前加一个减号“-”（在Tab键之后），标记为不管命令出不出错都认为是成功的。如：

```makefile
   clean:
            -rm -f *.o
```

还有一个全局的办法是，给make加上“-i”或是“--ignore-errors”参数，那么，Makefile中所有命令都会忽略错误。



## 十五、嵌套执行make

在一些大的工程中，我们会把我们不同模块或是不同功能的源文件放在不同的目录中，我们可以在每个目录中都书写一个该目录的Makefile，这有利于让我们的Makefile变得更加地简洁，而不至于把所有的东西全部写在一个Makefile中，这样会很难维护我们的Makefile，这个技术对于我们模块编译和分段编译有着非常大的好处。



例如，我们有一个子目录叫subdir，这个目录下有个Makefile文件，来指明了这个目录下文件的编译规则。那么我们总控的Makefile可以这样书写

```makefile
    #进入subdir目录执行make命令
    #定义$(MAKE)宏变量因为，我们的make可能需要一些参数，所以定义成一个变量
    subsystem:
            cd subdir && $(MAKE)  


    subsystem:
            $(MAKE) -C subdir

```



## 十六、定义命令包

如果Makefile中出现一些相同命令序列，那么我们可以为这些相同的命令序列定义一个变量。定义这种命令序列的语法以“define”开始，以“endef”结束，如：

```makefile
    define run-yacc
    yacc $(firstword $^)
    mv y.tab.c $@
    endef
```

定义好之后,就可以在其他地方使用，需要注意是的是不要和Makefile中的变量重名

```makefile
    foo.c : foo.y
            $(run-yacc)
```









## 十八、make的运行

### 1.make的退出码

make命令执行后有三个退出码：

​    0 —— 表示成功执行。
​    1 —— 如果make运行时出现任何错误，其返回1。
​    2 —— 如果你使用了make的“-q”选项，并且make使得一些目标不需要更新，那么返回2。

### 2.指定特定的Makefile

执行make 命令以后 依次寻找 “GNUmakefile”、“makefile”和“Makefile”。其按顺序找这三个文件，一旦找到，就开始读取这个文件并执行。

我们可以通过使用 ` make –f xxx.mk` 来指定特定的Makefile



## 十九、文件搜寻

在一些大的工程中，有大量的源文件，我们通常的做法是把这许多的源文件分类，并存放在不同的目录中。所以，当make需要去找寻文件的依赖关系时，你可以在文件前加上路径，但最好的方法是把一个路径告诉make，让make在自动去找。

Makefile文件中的特殊变量“VPATH”就是完成这个功能的，如果没有指明这个变量，make只会在当前的目录中去找寻依赖文件和目标文件。如果定义了这个变量，那么，make就会在当当前目录找不到的情况下，到所指定的目录中去找寻文件了。

​    VPATH = src:../headers

上面的的定义指定两个目录，“src”和“../headers”，make会按照这个顺序进行搜索。目录由“冒号”分隔。（当然，当前目录永远是最高优先搜索的地方）

另一个设置文件搜索路径的方法是使用make的“vpath”关键字（注意，它是全小写的），这不是变量，这是一个make的关键字，这和上面提到的那个VPATH变量很类似，但是它更为灵活。它可以指定不同的文件在不同的搜索目录中。这是一个很灵活的功能。它的使用方法有三种：

​    1、vpath <pattern> <directories>

​    为符合模式<pattern>的文件指定搜索目录<directories>。

​    2、vpath <pattern>

​    清除符合模式<pattern>的文件的搜索目录。

​    3、vpath

​    清除所有已被设置好了的文件搜索目录。

vapth使用方法中的<pattern>需要包含“%”字符。“%”的意思是匹配零或若干字符，例如，“%.h”表示所有以“.h”结尾的文件。<pattern>指定了要搜索的文件集，而<directories>则指定了<pattern>的文件集的搜索的目录。例如：

​    vpath %.h ../headers

该语句表示，要求make在“../headers”目录下搜索所有以“.h”结尾的文件。（如果某文件在当前目录没有找到的话）

我们可以连续地使用vpath语句，以指定不同搜索策略。如果连续的vpath语句中出现了相同的<pattern>，或是被重复了的<pattern>，那么，make会按照vpath语句的先后顺序来执行搜索。如：

​    vpath %.c foo
​    vpath %   blish
​    vpath %.c bar

其表示“.c”结尾的文件，先在“foo”目录，然后是“blish”，最后是“bar”目录。

​    vpath %.c foo:bar
​    vpath %   blish

而上面的语句则表示“.c”结尾的文件，先在“foo”目录，然后是“bar”目录，最后才是“blish”目录。







