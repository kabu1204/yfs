#include <linux/fs.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/module.h>

#include "yfs.h"

static struct kmem_cache *yfs_inode_cache;

int yfs_init_inode_cache(void){
    yfs_inode_cache = kmem_cache_create("yfs_inode_cache", sizeof(struct yfs_inode_info), 0, 0, NULL);
    if(!yfs_inode_cache)
        return -ENOMEM;
    return 0;
}

void yfs_destroy_inode_cache(void){
    kmem_cache_destroy(yfs_inode_cache);
}

static struct inode *yfs_alloc_inode(struct super_block *sb){
    struct yfs_inode_info* yi;
    yi = kmem_cache_alloc(yfs_inode_cache, GFP_KERNEL);
    if(!yi) return NULL;
    
    inode_init_once(&yi->v_inode);
    return &yi->v_inode;
}

static void yfs_destroy_inode(struct inode* inode){
    kmem_cache_free(yfs_inode_cache, container_of(inode, struct yfs_inode_info, v_inode));
}

static int yfs_write_inode(struct inode *inode, struct writeback_control *wbc){
    struct yfs_inode* disk_inode;

    pr_info("[yfs_write_inode] s_op->write_inode called\n");

    // disk_inode->i_mode = inode->i_mode;
    // disk_inode->i_uid = i_uid_read(inode);
    // disk_inode->i_gid = i_gid_read(inode);
    // disk_inode->i_size = inode->i_size;
    // disk_inode->i_ctime = inode->i_ctime.tv_sec;
    // disk_inode->i_atime = inode->i_atime.tv_sec;
    // disk_inode->i_mtime = inode->i_mtime.tv_sec;
    // disk_inode->i_blocks = inode->i_blocks;
    // disk_inode->i_nlink = inode->i_nlink;

    return 0;
}

static struct super_operations yfs_super_ops = {
    .alloc_inode = yfs_alloc_inode,
    .destroy_inode = yfs_destroy_inode,
    .write_inode = yfs_write_inode,
};

int yfs_fill_super(struct super_block *sb, void *data, int silent)
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

    inode = yfs_get_inode(sb, YFS_ROOT_INO, NULL, S_IFDIR | YFS_DEFAULT_MODE, 0);
    sb->s_root = d_make_root(inode);
    if (!sb->s_root){
        pr_err("[yfs_fill_super] failed to d_make_root\n");
        return -ENOMEM;
    }

    pr_info("[yfs_fill_super] fill_super finished\n");

    return 0;
}