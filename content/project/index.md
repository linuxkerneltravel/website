+++
title = "社区项目"
id = "contact"

+++
# Linux显微镜

本项目参加开源软件供应链点亮计划暑期 2020 项目活动，如您参与，请先仔细阅读[学生指南](https://isrc.iscas.ac.cn/summer2020/help/student.html)。

项目描述：LMP(Linux microscope)是一个基于bcc(BPF Compiler Collection)的Linux系统性能数据实时展示的web工具，它使用BPF(Berkeley Packet Filters)，也叫eBPF技术，提取Linux内核细粒度性能数据，展示在web界面，为运维人员提供参考。

1. 项目难度：低
2. 项目社区导师：陈莉君
3. 导师联系方式：cljcore@126.com
4. 合作导师联系方式（选填）：
5. 项目产出要求：
   - 可在前端页面选择要提取的性能指标，后台成功提取数据。
   - 完成项目的单机实现，本地成功展示云端系统、应用的性能数据。
   - 分布式实现，将LMP应用在分布式环境。
   - 使用bpf提取性能指标，包括但不限于进程管理、内存、文件系统、磁盘、网络，每个子系统提取的指标个数不少于5个。
6. 项目技术要求：
   - 了解Linux内核原理
   - 基本的 Linux 命令
   - ebpf、bcc技术
   - 具备三种语言，c,golang,python
   - web技术
   - 前端技术
7. 相关的开源软件仓库列表：
   - https://github.com/linuxkerneltravel/lmp  
   - https://gin-gonic.com/
   - https://github.com/iovisor/bcc