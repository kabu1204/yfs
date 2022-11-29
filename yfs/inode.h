#ifndef YFS_INODE_H
#define YFS_INODE_H

#include <linux/buffer_head.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>

struct inode* yfs_get_inode(struct super_block* sb, unsigned int ino, const struct inode *dir, umode_t mode, dev_t dev);

#endif