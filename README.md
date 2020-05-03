# 欢迎访问Linux内核之旅开源社区网站

**网站入口：[http://kerneltravel.net/](http://kerneltravel.net/)**

十多年前，陈老师和她的学生康华、陈逸飞等创办了“Linux内核之旅”（http://kerneltravel.net/）
网站，并撰写了电子杂志的系列文章。近年来，有把这个网站与微信平台打通的愿望，多年积累，大量的资料大都散落在各处，是时候梳理它们，让它们回归到自己的家园了。

如今，在陈老师和在腾讯工作十多年的许振文师兄的指导下，我们重新对Linux内核之旅进行打造，使之完全自由和开放，我们的运作方式与国际开源社区一样，所有人都可以共建Linux内核之旅开源社区，我们都是贡献者，也是受益者。

<img src="http://ww1.sinaimg.cn/large/005NFTS2ly1geayq0abxzj311j0gsav3.jpg"/>

您可以在我们开源社区网站上进行投稿，我们网站是使用hugo搭建的静态网站，您可以先配置好hugo和git这两个环境，将我们网站的git仓库fork到您的github，git pull 到本地后，您可以添加您的博客文章，添加您阅读陈老师出版书的读书笔记，甚至您还可以优化我们的网站，git push到您的git仓库后，再向我们的主仓库提交 pull requests，我们的管理员看到后会 review 您的提交，如果没有问题的话，就可以 merge 到我们的git主仓库啦。

Linux内核之旅开源社区网站git仓库地址：https://github.com/linuxkerneltravel/website

我们的许振文大师兄很贴心地贡献了参与Linux内核之旅开源社区的操作步骤，这是开源社区网站投稿的步骤，其它 git 仓库的参与步骤都是大同小异，一起来来看看吧！



## 前言

Linux 内核之旅的网站我们重构了，这次使用了 github 管理，hugo 作为站点管理工具。目标是能够让更多的同学参与进来，学习，分享，共同建设，让大家更方便高效的走 Linux 内核之旅。

>“Linux内核之旅”网站的大幅度改版，更是为热爱开源的Linuxer提供更广的舞台，大家的周报告，分享视频，相关代码，点点滴滴都将会通过Linux内核之旅网站，公众号，学堂在线，Github 以及B 站等平台全面的分享出来。Open，Free&Share，不仅仅是一个口号，落地生花之时，也是一个人从内而外的成长之际。-- 陈莉君教授。

所以这里也欢迎大家参与分享，这篇文章主要介绍怎么参与投稿。

## 投稿内容

我们是《Linux 内核之旅》开源社区，所以一切投稿还是以 Linux 内核为主，另外在软件开发理论，开源代码分享，社区文化方面也可以投稿。

## 投稿方式

目前我们是 github 的仓库来做管理，所以投稿的方式也是在 github 的仓库中直接提交 pr。具体提交 pr 的流程如下： ![img](https://mmbiz.qpic.cn/mmbiz_png/SeWfibBcBT0E6iaibhTvgicrT1N346nedtZTS4H1wkDyJpL3EXE25JjaRibYto2EHzadfnF8FsRG776aEHAQ5krM28A/640?wx_fmt=png&tp=webp&wxfrom=5&wx_lazy=1&wx_co=1)

## 详细步骤说明

### 1.fork 我们的站点项目到自己的仓库

站点仓库：https://github.com/linuxkerneltravel/website

fork 项目过程相对比较简单，在 https://github.com/linuxkerneltravel/website 页面右上角点击 fork 按钮即可， fork 到自己的空间。

我的 fork 后是这样的地址，因为我的空间已经有一个 `website` 的仓库了，所以这里就加了一个后缀来区别。https://github.com/helight/website-linuxkerneltravel

### 2.clone 主仓库到本地

https://github.com/linuxkerneltravel/website

```
git clone https://github.com/linuxkerneltravel/website helightxu$ cd website
```

### 3.设置自己的仓库开发代码提交上游关键

```
git remote add dev https://github.com/helight/website-linuxkerneltravel
```

### 4.新建分支，并且在新分支上修改提交代码

#### 4.1 代码更新

在每次新建分支之前一定要执行 git pull，使得 master 分支保持最新。

```
git pull
git checkout -b pr_intro
```

#### 4.2 编辑开发

社区站点是使用 hugo 搭建管理，所以大家需要在本地搭建使用 hugo 来预览稿件效果。这里有个中文帮助站点大家可以学习。（https://hugo.aiaide.com/）

这里以 hugo 新建一个博文为例进行介绍。首先使用下面的命令新建一个 markdown 文件。

```
hugo new blog/2020/submit_pr/index.md
```

然后进行博文撰写，撰写格式要求：

```
 1. 必须按照 `/blog/20xx/英文文章名称/index.md` 的路径格式创建文章。英文文章名称使用英文字母、下划线、连字符和数字，其它字符不接受。 
 2. 要求的内容格式一定是 markdown 的，其它格式内容暂时不接受。 
 3. 使用的图片一律保存在和 markdown 文件同级目录下的 imgs 文件夹中，如：`/blog/2020/submit_pr/imgs/pr.png`。 
 4. 图片的名称也一律使用英文命名，规则和上面一致。图片宽度不要超过900的宽度。  
 5. 图片大小在500k以内。
```

#### 4.3 编辑完成之后进行本地验证

这一步**非常重要**，一定要进行本地验证，避免文章有 markdown 语法、图片格式、文字错误等。所以一定要验证。

```
hugo server
```

看到上面信息就可以在本地浏览器中预览站点，看撰写的文字是否符合自己的预期。如果有问题可以修改后直接刷新看效果。

#### 4.4 编辑本机验证没有问题之后做本地提交。

```
git add content/blog/2020/submit_pr
git commit -m "add new blog submit_pr" -a
```

### 5.提交代码到 dev 上游仓库

这个 dev 上游就是上面设置的哈：git remote add dev https://github.com/helight/website-linuxkerneltravel

这种设置方式是可以把本地的修改按照 `dev` 标签提交到指定的另外一个仓库。我们一般是以主仓库作为我们工作目录，但是从主仓库的 `master` 分支创建出来的开发分支是不可以提交主仓库的，所以个人仓库就是这个分支提交的地方，提交之后在在个人仓库的分支和主仓库的 `master` 分支创建 `pr`。

```
git push dev
```

接下来就可以在这里查看代码了：https://github.com/helight/website-linuxkerneltravel。这里 `pr_intro` 这个分支就是刚刚提交的。

### 6.创建pr

在自己的个人仓库 https://github.com/helight/website-linuxkerneltravel 上面可以直接看到创建 pr 的按钮，直接创建就好了。

```
创建 pr之后，后面有修改直接提交到这个个人分支上就可以了，不用重复创建。
```

### 7.等待 reviewer 反馈和合并到主干

社区的管理员会对你提交的 pr 进行 review，review 后会提出修改点，或者 review 没有问题直接合到主干中。

另外如果提出问题，大家可以在这里讨论，并修改达成一致，并提交到这个分支上，最后再合到主干中。

## 总结

以上简单说了我们社区文章投稿的过程。希望大家多多参与共建《Linux 内核之旅》社区。