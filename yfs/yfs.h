#ifndef YFS_YFS_H
#define YFS_YFS_H

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

#define YFS_ROOT_INO 2

int yfs_init_inode_cache(void);
void yfs_destroy_inode_cache(void);

int yfs_fill_super(struct super_block *sb, void *data, int silent);

struct yfs_inode_info {
    struct inode v_inode;
};

struct yfs_inode {
    uint32_t i_mode;   /* File mode */
    uint32_t i_uid;    /* Owner id */
    uint32_t i_gid;    /* Group id */
    uint32_t i_size;   /* Size in bytes */
    uint32_t i_ctime;  /* Inode change time */
    uint32_t i_atime;  /* Access time */
    uint32_t i_mtime;  /* Modification time */
    uint32_t i_blocks; /* Block count */
    uint32_t i_nlink;  /* Hard links count */
    uint32_t ei_block;  /* Block with list of extents for this file */
    char i_data[32]; /* store symlink content */
};

#endif