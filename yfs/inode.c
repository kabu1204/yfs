#include "inode.h"
#include "linux/fs.h"
#include "linux/bio.h"
#include "linux/slab.h"
#include "../yssd/types.h"
#include "linux/genhd.h"
#include "yfs.h"

struct page* page;

extern struct file_operations yfs_dir_ops;
extern struct file_operations yfs_file_ops;
extern struct address_space_operations yfs_aops;

struct inode_operations yfs_inode_ops;

void y_end_bio(struct bio* bio){
    char *buf;
    pr_info("bio end\n");
    kfree(((struct y_io_req*)bio->bi_private)->key);
    kfree(bio->bi_private);
    buf = kmap_atomic(page);
    kunmap_atomic(buf);
    free_page((unsigned long)page_address(page));
    bio_put(bio);
}

struct inode* yfs_get_inode(struct super_block* sb, unsigned int ino, const struct inode *dir, umode_t mode, dev_t dev)
{
    struct inode* inode = NULL;
    struct bio* bio = NULL;
    struct y_io_req* req = NULL;
    if(!sb->s_bdev){
        pr_err("s_bdev == NULL!\n");
        goto out;
    }

    if(!sb->s_bdev->bd_disk){
        pr_err("s_bdev->bd_disk == NULL!\n");
        goto out;
    }

    pr_info("[yfs_get_inode] disk_name: %s\n", sb->s_bdev->bd_disk->disk_name);

    inode = iget_locked(sb, ino);
    if(!inode){
        pr_err("[yfs_get_inode] failed to iget inode\n");
        return ERR_PTR(-ENOMEM);
    }

    if(!(inode->i_state & I_NEW)){
        return inode;
    }

    inode->i_ino = ino;
    inode->i_sb = sb;
    inode->i_mapping->a_ops = &yfs_aops;
    inode_init_owner(inode, dir, mode);

    inode->i_atime = inode->i_mtime = inode->i_ctime = current_time(inode);

    switch (mode & S_IFMT) {
    default:
        init_special_inode(inode, mode, dev);
        break;
    case S_IFREG:
        pr_info("[yfs_get_inode] S_IFREG\n");
        inode->i_op = &yfs_inode_ops;
        inode->i_fop = &yfs_file_ops;
        break;
    case S_IFDIR:
        pr_info("[yfs_get_inode] S_IFDIR\n");
        inode->i_op = &yfs_inode_ops;
        inode->i_fop = &yfs_dir_ops;
        /* directory inodes start off with i_nlink == 2 (for "." entry) */
        inc_nlink(inode);
        break;
    case S_IFLNK:
        inode->i_op = &page_symlink_inode_operations;
        inode_nohighmem(inode);
        break;
    }

out:
    unlock_new_inode(inode);
    return inode;
}

static struct inode *yfs_new_inode(struct inode *dir, mode_t mode){
    struct inode* inode;
    struct yfs_inode_info* yi;
    struct super_block* sb;
    unsigned int ino;

    if (!S_ISDIR(mode) && !S_ISREG(mode) && !S_ISLNK(mode)) {
        pr_err(
            "File type not supported (only directory, regular file and symlink "
            "supported)\n");
        return ERR_PTR(-EINVAL);
    }

    sb = dir->i_sb;
    ino = get_next_ino();
    inode = yfs_get_inode(sb, ino, dir, mode, 0);
    if(!inode){
        pr_err("[yfs_new_inode] new inode failed\n");
    }

    return inode;
}

static int yfs_create(struct inode *dir, struct dentry *dentry, umode_t mode, bool excl)
{
    struct super_block* sb;
    struct inode* inode;
    struct bio* bio = NULL;
    struct y_io_req* req = NULL;
    sb = dir->i_sb;

    if(dir)
        pr_info("[yfs_create] creating %s, filename len: %d\n", dentry->d_name.name, dentry->d_name.len);

    inode = yfs_new_inode(dir, mode);
    if(!inode){
        pr_err("[yfs_create] failed to new inode\n");
        return -ENOMEM;
    }

    bio = bio_alloc(GFP_NOIO, 1);
    if(!bio) return -ENOMEM;
    req = kmalloc(sizeof(struct y_io_req), GFP_KERNEL);
    req->typ = SET;
    req->key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
    req->key->typ = 'm';
    req->key->ino = dir ? dir->i_ino : 0;   // parent ino
    req->key->len = dir ? dentry->d_name.len: 1;
    strncpy(req->key->name, dir?dentry->d_name.name:"/", req->key->len);
    bio->bi_end_io = y_end_bio;
    page = alloc_pages_current(GFP_KERNEL, 0);
    bio_add_page(bio, page, PAGE_SIZE, 0);
    bio->bi_disk = sb->s_bdev->bd_disk;
    bio->bi_private = req;
    bio_associate_blkg(bio);
    submit_bio(bio);

    /* Update stats and mark dir and new inode dirty */
    mark_inode_dirty(inode);
    dir->i_mtime = dir->i_atime = dir->i_ctime = current_time(dir);
    if (S_ISDIR(mode))
        inc_nlink(dir);
    mark_inode_dirty(dir);

    /* setup dentry */
    d_instantiate(dentry, inode);

    return 0;
}

static int yfs_mkdir(struct inode * dir, struct dentry * dentry, umode_t mode){
    return yfs_create(dir, dentry, mode | S_IFDIR, 0);
}

struct inode_operations yfs_inode_ops = {
    .create = yfs_create,
    .mkdir = yfs_mkdir,
    .lookup = simple_lookup,
};