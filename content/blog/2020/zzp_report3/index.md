---
title: "Linux's boot process explained"
date: 2020-12-3T16:50:24+08:00
author: "Ovidiu T."
keywords: ["process"]
categories : ["linux杂谈"]
banner : "img/blogimg/3.png"
summary : " about Linux's boot process."
---
```
Linux's boot process explained 



Linux's boot process explained 
Short history of the UNIX operating system

Linux is an implementation of the UNIX operating system concept. UNIX was derived from AT&T's "Sys V" (System 5). The initialization process is meant to control the starting and ending of services and/or daemons in a system, and permits different start-up configurations on different execution levels ("run levels").
Some Linux distribution, like SlackWare, use the BSD init system, developed at the University of California, Berkeley.
Sys V uses a much more complex set of command files and directives to determine which services are available at different levels of execution, than the BSD's do.

Booting the Linux operating system

The first thing a computer does on start-up is a primer test (POST - Power On Self Test). This way several devices are tested, including the processor, memory, graphics card and the keyboard. Here is tested the boot medium (hard disk, floppy unit, CD-ROMs). After POST, the loader from a ROM loads the boot sector, which in turn loads the operating system from the active partition.
The boot blocks is always at the same place: track 0, cylinder 0, head 0 of the device from which we're booting. This block contains a program called loader, which in Linux's case is LiLo (Linux Loader), or Grub (GNU Grub Unified Boot Loader), which actually boots the operating system. These loaders in Linux , in case of a multi-boot configuration (more operating systems on a computer), permit the selection of the operating system to be booted. Lilo and Grub are installed or at the MBR (Master Boot Record), or at the first sector of the active partition.
In the following we will refer to LiLO as boot loader. This is usually installed in the boot sector, also known as MBR. If the user decides to boot Linux, LiLo will try to load the kernel. Now I will present step-by-step LiLo's attempt to load the operating system.

1. In case of a multi-boot config, LiLo permits the user two choose an operating system from the menu. The LiLo settings are stored at /etc/lilo.conf. System administrators use this file for a very detailed finement of the loader. Here can be manually set what operating systems are installed, as well as the method for loading any of them. If on the computer there is only Linux, LiLo can be set to load directly the kernel, and skip the selection menu.

2. The Linux kernel is compressed, and contains a small bit, which will decompress it. Immediately after the first step begins the decompression and the loading of the kernel.

3. If the kernel detects that your graphics card supports more complex text modes, Linux allows the usage of them - this can be specified or during the recompilation of the kernel, or right inside Lilo, or other program, like rdev.

4. The kernel verifies hardware configuration (floppy drive, hard disk, network adapters, etc) and configures the drivers for the system. During this operation, several informative messages are shown to the user.

5. The kernel tries to mount the file system and the system files. The location of system files is configurable during recompilation, or with other programs - LiLo and rdev. The file system type is automatically detected. The most used file systems on Linux are ext2 and ext3. If the mount fails, a so-called kernel panic will occur, and the system will "freeze".
System files are usually mounted in read-only mode, to permit a verification of them during the mount. This verification isn't indicated if the files were mounted in read-write mode.

6. After these steps, the kernel will start init, which will become process number 1, and will start the rest of the system.

The init process

It's Linux's first process, and parent of all the other processes. This process is the first running process on any Linux/UNIX system, and is started directly by the kernel. It is what loads the rest of the system, and always has a PID of 1.

The initialization files in /etc/inittab

First time the initialization process (init) examines the file /etc/inittab to determine what processes have to be launched after. This file provides init information on runlevels, and on what process should be launched on each runlevel.
After that, init looks up the first line with a sysinit (system initialization) action and executes the specified command file, in this case /etc/rc.d/rc.sysinit. After the execution of the scripts in /etc/rc.d/rc.sysinit, init starts to launch the processes associated with the initial runlevel.
The next few lines in /etc/inittab are specific to the different execution (run-) levels. Every line runs as a single script (/etc/rc.d/rc), which has a number from 1 to 6 as argument to specify the runlevel.
The most used action in /etc/inittab is wait, which means init executes the command file for a specified runlevel, and then waits until that level is terminated.

The files in /etc/rc.d/rc.sysinit

The commands defined in /etc/inittab are executed only once, by the init process, every time when the operating system boots. Usually these scripts are running as a succession of commands, and usually realise the following:

1. Determine whether the system takes part of a network, depending on the content of /etc/sysconfig/network

2. Mount /proc, the file system used in Linux to determine the state of the diverse processes.

3. Set the system time in fuction to the BIOS settings, as well as realises other settings (setting of time zone, etc), stabilized and configured during the installation of the system.

4. Enables virtual memory, activating and mounting the swap partition, specified in /etc/fstab (File System Table)

5. Sets the host name for the network and system wide authentication, like NIS (Network Information Service), NIS+ (an improved version of NIS), and so on.

6. Verifies the root fily system, and if no problems, mounts it.

7. Verifies the other file systems specified in /etc/fstab.

8. Identifies, if case of, special routines used by the operating system to recognize installed hardware to configure Plug'n'Play devices, and to activate other prime devices, like the sound card, for example.

9. Verifies the state of special disk devices, like RAID (Redundant Array of Inexpensive Disks)

10. Mounts all the specified file systems in /etc/fstab.

11. Executes other system-specific tasks.

The /etc/rc.d/init.d directory

The directory /etc/rc.d/init.d contains all the commands which start or stop services which are associated with all the execution levels.
All the files in /etc/rc.d/init.d have a short name which describes the services to which they're associated. For example, /etc/rc.d/init.d/amd starts and stops the auto mount daemon, which mounts the NFS host and devices anytime when needed.

The login process

After the init process executes all the commands, files and scripts, the last few processes are the /sbin/mingetty ones, which shows the banner and log-in message of the distribution you have installed. The system is loaded and prepared so the user could log in.

Linux's execution levels

The execution levels represent the mode in which the computer operates. They are defined by a set of available services at any time they are started. The execution levels represent different ways Linux uses to be available to you, the user, or eventually the administrator.
As daily user you don't have to bother with the execution levels, although the multi-user level makes the services which you need while using Linux in a network (though in a transparent mode) available.
In the next few sentences I'll present the execution levels, one by one:

0: Halt (stops all running processes and executes shutdown)

1: Known under the name "Single-user mode". In this case the system runs with a reduced set of services and daemons. The root file system is mounted read-only. This runlevel is used when the others fail while booting.

2: On this level run the most of the services, with the exception of network services (httpd, named, nfs, etc). This execution level is ideal for the debug of network services, keeping the file system shared.

3: Complete multi-user mode, with network support enabled.

4: Unused, in most of the distributions. In Slackware this level is equivalent with 3, the only difference is that this has graphic login enabled.

5: Complete multi-user mode, with network and graphic subsystem support enabled.

6: Reboot. Stops all running processes and reboots the system to the initial execution level.

Modification of execution levels

The most used facility of init, and maybe the most confusing one, is the ability to move from an execution level to an other.
The system boots into a runlevel specified in /etc/inittab, or to a level specified at the LiLo prompt. To change the execution level, use the command init. For example, to change the execution level to 3, type

init 3

This stops most of the processes and takes the system into a multi-user mode with networking enabled. Attention, changing the init level might force several daemons used at the moment to stop!

The directories of execution levels

Every execution level has a directory with a symbolic links (symlinks) pointing to the corresponding scripts in /etc/rc.d/init.d. These directories are:

/etc/rc.d/rc0.d
/etc/rc.d/rc1.d
/etc/rc.d/rc2.d
/etc/rc.d/rc3.d
/etc/rc.d/rc4.d
/etc/rc.d/rc5.d
/etc/rc.d/rc6.d

The name of the symlinks are semnificative. It specifies which service has to be stopped, started and when. The links starting with an "S" are programmed to start in various execution levels. The links also have a number in their name (01-99). Now some examples of symlinks in the directory /etc/rc.d/rc2.d:

K20nfs -> ../init.d/nfs
K50inet -> ../init.d/inet
S60lpd -> ../init.d/lpd
S80sendmail -> ../init.d/sendmail

When operating systems change the execution level, init compares the list of the terminated processes (links which start with "K") from the directory of the current execution level with the list of processes which have to be started (starting with "S"), found in the destination directory.

Example:

When the system boots into runlevel 3, will execute all the corresponding links starting with "S", in an order accorind to their number:

/etc/rc.d/rc3.d/S60lpd start
/etc/rc.d/rc3.d/S80sendmail start
(and so on)

If the system now changes to runlevel 1, will execute:

/etc/rc.d/rc3.d/K20nfs stop
/etc/rc.d/rc3.d/K50inet stop
(presuming that nfs and inet are NOT in /etc/rc.d/rc1.d)

After that it will start all the processes mentioned in /etc/rc.d/rc1.d except which are already running. In this example there's a single one only:

/etc/rc.d/rc1.d/S00single

Changing the current execution level

To change the current execution level for example to level 3, edit /etc/inittab in a text editor, and edit the following line:

id:3:initdefault:

(do not change the initial runlevel to 0 or 6!)

Booting into an alternative execution level

At the LiLo prompt you have to write the number of the wanted execution level, before booting the operating system. This way to boot into the third level, type for example:


linux 3


Eliminating a service from an execution level

To disable a service from a runlevel, you might simply delete or modify the corresponding symlink.
For example, to disable pcmcia, and don't start in the future, type:


rm /etc/rc.d/rc3.d/S45pcmcia


Adding a service to an execution level

To add a service, it is needed to create a symlink pointing to the corresponding scripts in /etc/rc.d/init.d. After the symlink is created, be sure to assign it a number, so it would be started in the right time:

To add "lpd" to runlevel 3, type:


ln -s /etc/rc.d/init.d/lpd /etc/rc.d/rc3.d/S64lpd




P.S.: This article has been originally written by Ovidiu T. He gave me the permission to translate and publish it, and since I found the article interesting, I did that way as well, although I don't treat myself responsible about the content of it.
```