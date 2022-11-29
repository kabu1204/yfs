#include "inode.h"
#include "linux/fs.h"
#include "linux/bio.h"
#include "linux/slab.h"
#include "../yssd/types.h"
#include "linux/genhd.h"

struct page* page;

void y_end_bio(struct bio* bio){
    char *buf;
    pr_info("bio end\n");
    kfree(bio->bi_private);
    buf = kmap_atomic(page);
    pr_info("read value: %s\n", buf);
    kunmap_atomic(buf);
    free_page((unsigned long)page_address(page));
    bio_put(bio);
}

struct inode* yfs_get_inode(struct super_block* sb, unsigned int ino, const struct inode *dir, umode_t mode, dev_t dev)
{
    struct inode* inode;
    if(!sb->s_bdev){
        pr_err("s_bdev == NULL!\n");
        goto out;
    }

    if(!sb->s_bdev->bd_disk){
        pr_err("s_bdev->bd_disk == NULL!\n");
        goto out;
    }

    pr_info("[yfs_get_inode] disk_name: %s\n", sb->s_bdev->bd_disk->disk_name);

    struct bio *bio = bio_alloc(GFP_NOIO, 1);
    if(!bio) return NULL;
    struct y_io_req* req = kmalloc(sizeof(struct y_io_req), GFP_KERNEL);
    req->typ = GET_FIRST_BLOCK;
    req->tid = 0;
    req->key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
    req->key->typ = METADATA;
    req->key->ino = 123;
    strcpy(req->key->name, "/home/ycy");
    bio->bi_end_io = y_end_bio;

    page = alloc_pages_current(GFP_KERNEL, 0);

    bio_add_page(bio, page, PAGE_SIZE, 0);

    bio->bi_disk = sb->s_bdev->bd_disk;
    bio->bi_private = req;

    bio_associate_blkg(bio);

    submit_bio(bio);
out:
    inode = new_inode(sb);
    if(!inode){
        pr_err("failed to new_inode\n");
        return NULL;
    }
    // TODO: set inode attributes
    return inode
}