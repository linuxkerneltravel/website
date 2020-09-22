---
title: "2.6驱动程序-字符驱动"
date: 2020-09-22T08:52:41+08:00
author: "helight0"
keywords: ["Linux2.6","字符驱动"]
categories : ["走进内核"]
banner : "img/blogimg/char_driver_szp_no1.jpg"
summary : "程序chardev.c是字符驱动程序，是以内核模块的形式插入内核的，所以编译方法和内核模块的编译方法一致。"
---
驱动程序： 
```C
# include <linux/module.h>
# include <linux/fs.h>
# include <linux/uaccess.h>
# include <linux/init.h>
# include <linux/cdev.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Helight");
#define DP_MAJOR 250 /*the major number of the chardev*/ 
#define DP_MINOR 0 /*the minor number of the chardev*/ 
static int char_read(struct file *filp, char __user *buffer, size_t, loff_t *); 
/*the read operation of the chardev----read the data from kernel*/
static int char_open(struct inode *, struct file *);                                                                                                                                            /*open the chardev*/
static int char_write(struct file *filp, const char __user *buffer, size_t, loff_t *);                                                                                                          /*write data to kernel*/
static int char_release(struct inode *, struct file *);                                                                                                                                         /*release the chardev*/
static int chropen;                                                                                                                                                                             /*the chardev open or not*/
struct cdev *chardev;                                                                                                                                                                           /*define a char device*/
static int len;
static const struct file_operations char_ops = {
    .read = char_read,
    .write = char_write,
    .open = char_open,
    .release = char_release,
};
static int __init char_init(void)
{
    dev_t dev;
    printk(KERN_ALERT "Initing......\n");
    dev = MKDEV(DP_MAJOR, DP_MINOR);
    chardev = cdev_alloc();
    if (chardev == NULL)
    {
        return -1;
    }
    if (register_chrdev_region(dev, 10, "chardev"))
    {
        printk(KERN_ALERT "Register char dev error\n");
        return -1;
    }
    chropen = 0;
    len = 0;
    cdev_init(chardev, &char_ops);
    if (cdev_add(chardev, dev, 1))
    {
        printk(KERN_ALERT "Add char dev error\n");
    }
    return 0;
}
static int char_open(struct inode *inode, struct file *file)
{
    if (chropen == 0)
        chropen++;
    else
    {
        printk(KERN_ALERT "Another process open the char device\n");
        return -1;
    }
    try_module_get(THIS_MODULE);
    return 0;
}
static int char_release(struct inode *inode, struct file *file)
{
    chropen--;
    module_put(THIS_MODULE);
    return 0;
}
static int char_read(struct file *filp, char __user *buffer, size_t length, loff_t *offset)
{
    if (length < 12)
        if (!copy_to_user(buffer, "hello world!", length))
        {
            return 0;
        }
}
else
{
    if (!copy_to_user(buffer, "hello world!", strlen("hello world!")))
    {
        return 0;
    }
}
return -1;
}
static int char_write(struct file *filp, const char __user *buffer, size_t length, loff_t *offset) { return 0; }
static void __exit module_close(void)
{
    len = 0;
    printk(KERN_ALERT "Unloading..........\n");
    unregister_chrdev_region(MKDEV(DP_MAJOR, DP_MINOR), 10);
    cdev_del(chardev);
}
module_init(char_init);
module_exit(module_close);

```
用户测试程序：

```C
/* main.c*/ 
# include <stdio.h>
# include <fcntl.h>
# include <unistd.h> 
int main(void)
{
    int testdev;
    int i, rf = 0;
    char buf[15];
    memset(buf, 0, sizeof(buf));
    testdev = open("/dev/chardev0", O_RDWR);
    if (testdev == -1)
    {
        perror("open\n");
        exit(0);
    }
    rf = read(testdev, buf, 12);
    if (rf < 0)
        perror("read error\n");
    printf("R:%s\n", buf);
    close(testdev);
    return 0;
}

```
 编译加载和使用： 

 <1>程序chardev.c是字符驱动程序，是以内核模块的形式插入内核的，所以编译方法和内核模块的编译方法一致。 

 <2>模块的加载和卸载也和上面所述的内核模块的加载和卸载方法一致。

 <3>设备节点的创建，mknod /dev/chardev0 c 250 0 

    命令解释： mknod是建立设备节点的命令；

    /dev/chardev0：在/dev/目录下建立chardev0这样一个节点。

    c:这个节点是指向一个字符设备的节点 

    250：这个设备的主设备号 0：次设备号 

 <4>编译用户程序gcc -o chardev_test main.c 

 <5>运行chmod 666 /dev/chardev0 使其它用户也可以对这个设备进行读写操作，否则只有root用户可以对它进行读写。 

 <6>运行chardev_test，如果没有什么问题的话应该要输出这几个字符。 R：hello world!。