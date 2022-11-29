#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include "internal.h"
#include "inode.h"

#define YFS_NAME "yfs"
#define YFS_MAGIC 0xcafe
#define YFS_MAX_FILE_SIZE 1<<28 // 256MB
#define YFS_DEFAULT_MODE 0755

static unsigned long once = 0;

static const struct super_operations yfs_super_ops = {
    .statfs = simple_statfs,
};

static int yfs_fill_super(struct super_block *sb, void *data, int silent)
{
    struct yfs_superblock_info *fsi;
    struct inode *inode;
    int res;

    fsi = kmalloc(sizeof(struct yfs_superblock_info), GFP_KERNEL);
    sb->s_fs_info = fsi;
    if (!fsi)
        return -ENOMEM;

    sb->s_maxbytes          = YFS_MAX_FILE_SIZE;
    sb->s_blocksize         = PAGE_SIZE;
    sb->s_blocksize_bits    = PAGE_SHIFT;
    sb->s_magic             = YFS_MAGIC;
    sb->s_op                = &yfs_super_ops;
    sb->s_time_gran         = 1;

    inode = yfs_get_inode(sb, 0, NULL, S_IFDIR | YFS_DEFAULT_MODE, 0);
    sb->s_root = d_make_root(inode);
    if (!sb->s_root){
        pr_err("failed to d_make_root\n");
        return -ENOMEM;
    }

    return 0;
}

struct dentry *yfs_mount(struct file_system_type *fs_type,
                              int flags,
                              const char *dev_name,
                              void *data)
{
    struct dentry *dentry =
        mount_bdev(fs_type, flags, dev_name, data, yfs_fill_super);
    if (IS_ERR(dentry))
        pr_err("'%s' mount failure\n", dev_name);
    else
        pr_info("'%s' mount success\n", dev_name);

    return dentry;
}

void yfs_kill_sb(struct super_block *sb)
{
    kfree(sb->s_fs_info);
    
    kill_block_super(sb);

    pr_info("unmounted disk\n");
}

static struct file_system_type yfs_file_system_type = {
    .owner = THIS_MODULE,
    .name = YFS_NAME,
    .mount = yfs_mount,
    .kill_sb = yfs_kill_sb,
    .fs_flags = FS_REQUIRES_DEV,
    .next = NULL,
};

static int __init yfs_init(void)
{
    if(test_and_set_bit(0, &once)) return 0;
    
    return register_filesystem(&yfs_file_system_type);
}

static void __exit yfs_exit(void)
{
    int res;
    
    res = unregister_filesystem(&yfs_file_system_type);
    if(res<0){
        pr_err("failed to unregister yfs\n");
        return;
    }

    pr_info("module unloaded\n");
}

module_init(yfs_init);
module_exit(yfs_exit);
MODULE_LICENSE("GPL");