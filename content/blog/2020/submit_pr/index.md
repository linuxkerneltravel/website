---
title: "如何给《Linux内核之旅社区》投稿"
date: 2020-04-11T14:31:20+08:00
categories: ["社区"]
banner : "img/logo.png"
summary: "Linux 内核之旅的网站我们重构了，这次使用了 github 管理，hugo 作为站点管理工具。目标是能够让更多的同学参与进来，学习，分享，共同建设，让大家更方便高效的走 Linux 内核之旅。"
draft: false
---

## 前言
Linux 内核之旅的网站我们重构了，这次使用了 github 管理，hugo 作为站点管理工具。目标是能够让更多的同学参与进来，学习，分享，共同建设，让大家更方便高效的走 Linux 内核之旅。
    “Linux内核之旅”网站的大幅度改版，更是为热爱开源的Linuxer提供更广的舞台，大家的周报告，分享视频，相关代码，点点滴滴都将会通过Linux内核之旅网站，公众号，学堂在线，Github 以及B 站等平台全面的分享出来。Open，Free&Share，不仅仅是一个口号，落地生花之时，也是一个人从内而外的成长之际。  -- 陈丽君教授。
所以这里也欢迎大家参与分享，这篇文章主要介绍怎么参与投稿。

## 投稿内容
我们是《Linux 内核之旅》社区，所以一切投稿还是以 Linux 内核为主，另外在软件开发理论，开源代码分享，社区文化方面也可以投稿。

## 投稿方式
目前我们是 github 的仓库来做管理，所以投稿的方式也是在 github 的仓库中直接提交 pr。具体提交 pr 的流程如下：
![](imgs/pr.png)

## 详细步骤说明
### 1.fork 我们的站点项目到自己的仓库
站点仓库： [https://github.com/linuxkerneltravel/website](https://github.com/linuxkerneltravel/website)

fork 项目过程相对比较简单，在 https://github.com/linuxkerneltravel/website 页面右上角点击 fork 按钮即可， fork 到自己的空间。

我的 fork 后是这样的地址，因为我的空间已经有一个 `website` 的仓库了，所以这里就加了一个后缀来区别。
https://github.com/helight/website-linuxkerneltravel


### 2.clone 主仓库到本地
[git remote add dev https://github.com/helight/website-linuxkerneltravel](https://github.com/linuxkerneltravel/website)

```sh
 helightxu$ git clone https://github.com/linuxkerneltravel/website
 helightxu$ cd website                                                                           
```
### 3.设置自己的仓库开发代码位提交上游关键
```sh
helightxu$ git remote add dev https://github.com/helight/website-linuxkerneltravel
```

### 4.新建分支，并且在新分支上修改提交代码
1. 在每次新建分支之前一定要执行 git pull，是的 master 分支保持最新。
```sh
helightxu$ git pull 
helightxu$ git checkout -b pr_intro
Switched to a new branch 'pr_intro'
helightxu$ 
```
2. 编辑开发
这里以 hugo 新建一个博文为例进行介绍。首先使用下面的命令新建一个 markdown 文件。
```sh
helightxu$ hugo new blog/2020/submit_pr/index.md                          ✔   pr_intro
/Users/helightxu/helight_doc/website-linuxkerneltravel/content/blog/2020/submit_pr/index.md created
helightxu$
```
然后进行博文撰写，撰写格式要求：
   1. 必须按照 `/blog/20xx/英文文章名称/index.md` 的路径格式创建文章。英文文章名称使用英文字母、下划线、连字符和数字，其它字符不接受
   2. 要求的内容格式一定是 markdown 的，其它格式内容暂时不接受。
   3. 使用的图片一律保存在和 markdown 文件同级目录下的 imgs 文件夹中，如：`/blog/2020/submit_pr/imgs/pr.png`。

1. 编辑完成之后做本地提交。
```sh
 $ git commit -m "add create pr" -a
 [add_creater_pr 6507547] add create pr
 5 files changed, 53 insertions(+)
 create mode 100644 Create_pr.md
 create mode 100644 static/imgs/create_pr/fork_project1.png
 create mode 100644 static/imgs/create_pr/fork_project2.png
 create mode 100644 static/imgs/create_pr/fork_project3.png
 create mode 100644 static/imgs/create_pr/fork_project4.png
提交代码到 dev 上游仓库，这个 dev 就是上面设置的
$ git push dev                                                                   
 Enumerating objects: 11, done.
 Counting objects: 100% (11/11), done.
 Delta compression using up to 12 threads
 Compressing objects: 100% (8/8), done.
 Writing objects: 100% (10/10), 900.36 KiB | 20.01 MiB/s, done.
 Total 10 (delta 0), reused 0 (delta 0)
 remote: Processing changes: done
 remote: Updating references: 100% (1/1)
 To https://git.code.oa.com/helightxu/community
 * [new branch]      add_creater_pr -> add_creater_pr
查看代码
这里 add_creater_pr 这个分支就是刚刚提交的。

创建pr
在自己的仓库站点，这里和 github 不一样，一定注意，是在自己 fork 来的仓库，我这里是 https://git.code.oa.com/helightxu/community。

点击左边的 Merge Requests 菜单，进行创建

看下图，左边是源分支（也就是自己的仓库分支），右边是目标分支（也就是主仓库的master分支），再点左下角的 Compare branches 进行创建

这一步主要是信息确认和pr信息填写，这里主要是填写为什么要提交这个pr，主要功能是什么，解决什么问题，确认和填写完成之后，点击 Submit new merge request 进行创建。

创建后会直接转跳到主仓库的页面。

接下来就等待reviewer反馈和合并到主干了。
　