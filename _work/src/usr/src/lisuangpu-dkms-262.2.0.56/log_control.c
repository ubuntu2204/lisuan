/*
 * @Copyright   Copyright (c) Lisuan Tech. All Rights Reserved
 * @License     Dual MIT/GPLv2
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/atomic.h>
#include "log_control.h"

int log_enabled = 0;

static ssize_t log_control_write(struct file *file, const char __user *buf, size_t count, loff_t *pos) {
    char buffer[2] = {0};
    if (copy_from_user(buffer, buf, count)) {
        return -EFAULT;
    }
    buffer[count] = '\0';

    if (buffer[0] == '1') {
        log_enabled = 1;
        printk(KERN_INFO "Logging enabled.\n");
    } else if (buffer[0] == '0') {
        log_enabled = 0;
        printk(KERN_INFO "Logging disabled.\n");
    } else {
        printk(KERN_INFO "Invalid input. Use '1' to enable or '0' to disable logging.\n");
    }

    return count;
}

static ssize_t log_control_read(struct file *file, char __user *buf, size_t count, loff_t *pos) {
    char buffer[2];
    if (*pos > 0 || count < 2) {
        return 0;  // 文件指针已移动或缓冲区太小
    }

    sprintf(buffer, "%d", log_enabled);
    if (copy_to_user(buf, buffer, strlen(buffer) + 1)) {
        return -EFAULT;
    }

    *pos += strlen(buffer) + 1;  // 更新文件指针
    return strlen(buffer) + 1;
}


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,6,0)
static struct  proc_ops proc_fops = {
    .proc_write = log_control_write,
    .proc_read = log_control_read,
};

#else
static const struct file_operations proc_fops = {
    .write = log_control_write,
    .read  = log_control_read,
};

#endif




// static const struct file_operations log_control_fops = {
//     .owner = THIS_MODULE,
//     .write = log_control_write,
//     .read = log_control_read,
// };

// static int __init log_control_init(void) {
//     proc_create("log_control", 0666, NULL, &proc_fops);
//     printk(KERN_INFO "Log control module loaded.\n");
//     return 0;
// }

// static void __exit log_control_exit(void) {
//     remove_proc_entry("log_control", NULL);
//     printk(KERN_INFO "Log control module unloaded.\n");
// }

// module_init(log_control_init);
// module_exit(log_control_exit);

// MODULE_LICENSE("GPL");
// MODULE_DESCRIPTION("Log Control Module");
// MODULE_AUTHOR("Your Name");

// // 导出 log_enabled 变量
// EXPORT_SYMBOL(log_enabled);

void gf_kmdEvent_log_control_init(void) {
    proc_create("log_control", 0666, NULL, &proc_fops);
    printk(KERN_INFO "Log control module loaded.\n");
}

void gf_kmdEvent_log_control_exit(void) {
    remove_proc_entry("log_control", NULL);
    printk(KERN_INFO "Log control module unloaded.\n");
}
