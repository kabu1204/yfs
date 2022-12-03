#include <linux/init.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/fs.h>
#include <linux/genhd.h>
#include <linux/blkdev.h>
#include <linux/buffer_head.h>
#include <linux/blk-mq.h>
#include <linux/hdreg.h>
#include <linux/rbtree.h>
#include <linux/random.h>
#include "rbkv.h"
#include "types.h"

#define YSSD_MAJOR 240
#define YSSD_DEV_NAME "yssd"

static unsigned long n_sectors;
static unsigned long n_bytes;
static unsigned long n_pages;

static char* yssd_file = "yssd.data";
module_param(yssd_file, charp, 0660);
MODULE_PARM_DESC(yssd_file, "file to be treated as a pseudo ssd");

static struct file* fp;
static char g_buf[PAGE_SIZE+1];

static struct yssd_block_dev {
    int yssd_number;
    sector_t capacity;
    struct blk_mq_tag_set tag_set;
    struct request_queue *queue;
    struct gendisk *gdisk;
} block_device;

static int blockdev_open(struct block_device *dev, fmode_t mode)
{
    pr_info("yssd open\n");
    return 0;
}

static void blockdev_release(struct gendisk *gdisk, fmode_t mode)
{
    pr_info("yssd release\n");
}

int blockdev_ioctl(struct block_device *bdev, fmode_t mode, unsigned cmd, unsigned long arg)
{
    pr_info("ioctl cmd 0x%08x\n", cmd);
    return -ENOTTY;
}

static int y_bio_get(char* buf, unsigned int max_len, const struct y_key* key, unsigned int len, unsigned off){
    // TODO: implement me
    pr_info("yssd GET\n");
    print_y_key(key);
    return 0;
}

static int y_bio_set(char* buf, unsigned int max_len, const struct y_key* key, unsigned int len, unsigned off){
    // TODO: implement me
    pr_info("yssd SET\n");
    return 0;
}

static int y_bio_del(char* buf, unsigned int max_len, const struct y_key* key, unsigned int len, unsigned off){
    // TODO: implement me
    pr_info("yssd DEL\n");
    return 0;
}

static int y_bio_get_first_block(char* buf, unsigned int max_len, const struct y_key* key, unsigned int len, unsigned off){
    // TODO: implement me
    pr_info("yssd GET_FIRST_BLOCK\n");
    print_y_key(key);
    strcpy(buf, "This is first block.");
    return 0;
}

static void yssd_read_phys_page(char* buf, unsigned long page_no){
    loff_t off=page_no*PAGE_SIZE;
    size_t sz;
    if(likely(page_no<n_pages)){
        sz = kernel_read(fp, buf, PAGE_SIZE, &off);
        if(unlikely(sz != PAGE_SIZE)){
            pr_warn("kernel_read %ld bytes != PAGE_SIZE\n", sz);
        }
        return;
    } else {
        pr_warn("page_no %ld >= n_pages(%ld)\n", page_no, n_pages);
    }
}

static void yssd_write_phys_page(char* buf, unsigned long page_no){
    loff_t off=page_no*PAGE_SIZE;
    size_t sz;
    if(likely(page_no<n_pages)){
        sz = kernel_write(fp, buf, PAGE_SIZE, &off);
        if(unlikely(sz != PAGE_SIZE)){
            pr_warn("kernel_read %ld bytes != PAGE_SIZE\n", sz);
        }
        return;
    } else {
        pr_warn("page_no %ld >= n_pages(%ld)\n", page_no, n_pages);
    }
}

static int yssd_load_file(void){
    loff_t off=0;
    ssize_t sz;
    char* full_path;

    fp = filp_open(yssd_file, O_RDWR, 0644);
    if(IS_ERR(fp)){
        pr_err("failed to open file: %s\n", yssd_file);
        return -EIO;
    }

    full_path = dentry_path_raw(fp->f_path.dentry, g_buf, PAGE_SIZE);
    pr_info("file path: %s\n", full_path);
    n_bytes = i_size_read(file_inode(fp));
    n_sectors = n_bytes/SECTOR_SIZE;
    n_pages = n_bytes/PAGE_SIZE;
    pr_info("Total space: %ld MB\n", n_bytes/(1<<20));
    pr_info("n_sectors:   %ld \n", n_sectors);
    pr_info("n_pages:     %ld \n", n_pages);

    
    sz = kernel_write(fp, "This is first block.", 20, &off);
    pr_info("bytes wrote: %ld\n", sz);
    pr_info("off after wrote: %lld\n", off);
    off = 0;
    sz = kernel_read(fp, g_buf, PAGE_SIZE, &off);
    pr_info("bytes read: %ld\n", sz);
    pr_info("off after read: %lld\n", off);
    pr_info("g_buf after read: %s\n", g_buf);

    return 0;
}


static void yssd_close_file(void){
    filp_close(fp, NULL);
}

static blk_qc_t y_make_request(struct request_queue* q, struct bio* bio){
    struct y_io_req* req = bio->bi_private;
	struct yssd_block_dev *dev = bio->bi_disk->private_data;
	struct bio_vec bvec;
	sector_t sector;
	struct bvec_iter iter;
    char* addr;
    unsigned int len;
    unsigned int off;
    int res;

	sector = bio->bi_iter.bi_sector;

	bio_for_each_segment(bvec, bio, iter) {
		len = bvec.bv_len;
        off = bvec.bv_offset;

        switch(req->typ) {
        case GET:
            if(req->tid==0){
                addr = kmap_atomic(bvec.bv_page);
                res = y_bio_get(addr+off, len, req->key, req->len, req->off);
                kunmap_atomic(addr);
            }
            break;
        case SET:
            if(req->tid==0){
                addr = kmap_atomic(bvec.bv_page);
                res = y_bio_set(addr+off, len, req->key, req->len, req->off);
                kunmap_atomic(addr);
            }
            break;
        case DEL:
            if(req->tid==0){
                addr = kmap_atomic(bvec.bv_page);
                res = y_bio_del(addr+off, len, req->key, req->len, req->off);
                kunmap_atomic(addr);
            }
            break;
        case GET_FIRST_BLOCK:
            addr = kmap_atomic(bvec.bv_page);
            res = y_bio_get_first_block(addr+off, len, req->key, req->len, req->off);
            kunmap_atomic(addr);
            break;
        default:
            break;
        };

		if(res < 0){
            addr = kmalloc(sizeof(struct y_key)+24, GFP_KERNEL);
            sprint_y_key(addr, req->key);
            pr_err("failed to perform bio, key: %s", addr);
            goto io_error;
        }
		sector += len >> SECTOR_SHIFT;
	}

	bio_endio(bio);
	return BLK_QC_T_NONE;
io_error:
	bio_io_error(bio);
	return BLK_QC_T_NONE;
}

/* Set block device file I/O */
static struct block_device_operations y_blk_ops = {
    .owner = THIS_MODULE,
    .open = blockdev_open,
    .release = blockdev_release,
    .ioctl = blockdev_ioctl
};

static int create_block_dev(struct yssd_block_dev *dev){
    dev->gdisk = alloc_disk(1);
    if(!dev->gdisk){
        pr_err("failed to alloc_disk\n");
        return -EBUSY;
    }

    dev->gdisk->major = YSSD_MAJOR;
    dev->gdisk->first_minor = 0;
    dev->gdisk->fops = &y_blk_ops;
    dev->gdisk->queue = dev->queue;
    dev->gdisk->private_data = dev;
    snprintf(dev->gdisk->disk_name, 32, "yssd_disk0");
    set_capacity(dev->gdisk, n_sectors);

    add_disk(dev->gdisk);
    return 0;
}

static void delete_block_dev(struct yssd_block_dev* dev){
    if(dev->gdisk){
        del_gendisk(dev->gdisk);
        put_disk(dev->gdisk);
        return;
    }
}

static int __init yssd_init(void)
{
    int res;

    res = register_blkdev(YSSD_MAJOR, YSSD_DEV_NAME);
    if(res<0){
        pr_err("failed to register yssd device\n");
        return -EBUSY;
    }

    block_device.queue = blk_alloc_queue(GFP_KERNEL);
    blk_queue_make_request(block_device.queue, y_make_request);

    res = create_block_dev(&block_device);
    if(res<0){
        pr_err("failed to create_block_dev\n");
        return res;
    }

    yssd_load_file();

    pr_info("YSSD init\n");

    test_y_rbkv_insert();
    test_y_rbkv_update();

    return 0;
}

static void __exit yssd_exit(void)
{
    delete_block_dev(&block_device);
    unregister_blkdev(YSSD_MAJOR, YSSD_DEV_NAME);
    yssd_close_file();
    pr_info("YSSD exit\n");
}

module_init(yssd_init);
module_exit(yssd_exit);
MODULE_LICENSE("GPL");