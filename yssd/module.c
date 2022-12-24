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
#include "heap.h"
#include "lsmtree.h"
#include "mem_index.h"
#include "rbkv.h"
#include "types.h"
#include "kv.h"
#include "value_log.h"
#include "linux/delay.h"

#define YSSD_MAJOR 240
#define YSSD_DEV_NAME "yssd"

static char* yssd_file = "yssd.data";
module_param(yssd_file, charp, 0660);
MODULE_PARM_DESC(yssd_file, "file to be treated as a pseudo ssd");

static char g_buf[PAGE_SIZE+1];

struct file* fp;
unsigned long n_sectors;
unsigned long n_bytes;
unsigned long n_pages;

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

static void test_hash(void){
    struct y_key k1, k2, k3;
    k1.ino = 2;
    k1.typ = 'm';
    strcpy(k1.name, "yssd");
    k1.len = strlen(k1.name);
    k2 = k1;
    k3 = k1;
    k2.ino = 11;
    k3.len = 6;
    pr_info("k1.len: %d\n", k1.len);
    pr_info("k1[k1.len]==0: %d\n", k1.name[k1.len]==0);
    pr_info("k1[k1.len-1]: %c\n", k1.name[k1.len-1]);
    pr_info("k1.hash: %lx\n", y_key_hash(&k1));
    pr_info("k2.hash: %lx\n", y_key_hash(&k2));
    pr_info("k3.hash: %lx\n", y_key_hash(&k3));
    pr_info("after hash: k1.name: %s\n", k1.name);
}

static void test_dump(void){
    struct y_key key;
    struct y_value val;
    struct vlog_node node;
    unsigned long p;
    val.buf = kmalloc(256, GFP_KERNEL);
    key.ino = 123;
    key.len = 4;
    key.typ = 'm';
    strcpy(key.name, "key1");
    val.len = 256;
    memcpy(val.buf, "hello, yssd.", 12);
    node.key = key;
    node.v = val;

    p = vlog_dump_size(&key, &val);
    pr_info("vlog_dump_size: %lu\n", p);
    p = vlog_node_dump(&node, g_buf);
    pr_info("vlog_node_dump: %lu\n", p);

    kfree(val.buf);
}

static void test_kv(void){
    struct y_key* key;
    struct y_value* val1, val2;
    val2.buf = NULL;
    pr_info("val2.buf=%p\n", val2.buf);
    key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
    val1 = kmalloc(sizeof(struct y_value), GFP_KERNEL);
    val1->buf = kmalloc(12, GFP_KERNEL);
    // val2.buf = kmalloc(12, GFP_KERNEL);
    val1->len = 12;
    memcpy(val1->buf, "hello, yssd.", val1->len);
    key->ino = 123;
    key->len = 4;
    key->typ = 'm';
    strcpy(key->name, "key1");    
    kv_set(key, val1);
    kv_get(key, &val2);
    memcpy(g_buf, val2.buf, 12);
    g_buf[val2.len]='\0';
    pr_info("kv_get: v->len=%u, v->buf=%s\n", val2.len, g_buf);
    kfree(key);
    kfree(val2.buf);
    kfree(val1->buf);
    kfree(val1);
}

static void test_kv_flush(void){
    struct y_key* key;
    struct y_value* val;
    int i;
    for(i=0;i<32768*2;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i;
        key->len = 4;
        key->typ = 'm';
        strcpy(key->name, "key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        kv_set(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
}

static void test_kv_flush_get(void){
    struct y_key* key;
    struct y_value* val;
    int i;
    unsigned long start, end;
    for(i=0;i<32768;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i;
        key->len = 4;
        key->typ = 'm';
        strcpy(key->name, "key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        kv_set(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
    start = ktime_get_ns();
    for(i=0;i<32768;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i;
        key->len = 4;
        key->typ = 'm';
        strcpy(key->name, "key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        kv_get(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
    end = ktime_get_ns();
    pr_info("read time cost: %luns/op\n", (end-start)/32768);
}

static void test_kv_flush_update(void){
    struct y_key* key;
    struct y_value* val;
    int i, t;
    for(i=0;i<32768*5;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i;
        key->len = 4;
        key->typ = 'm';
        strcpy(key->name, "key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        *(int*)(val->buf + 12) = i;
        kv_set(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
    for(i=0;i<32768;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i;
        key->len = 4;
        key->typ = 'm';
        strcpy(key->name, "key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        kv_get(key, val);
        t = *(int*)(val->buf + 12);
        if(t!=(32768*4+i)){
            pr_info("err %d %d\n", (32768*4+i), t);
        }
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
}

static void test_kv_gc(void){
    struct y_key* key;
    struct y_value* val;
    int i, t;
    for(i=0;i<32768*5;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i;
        key->len = 4;
        key->typ = 'm';
        strcpy(key->name, "key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        *(int*)(val->buf + 12) = i;
        kv_set(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
    for(i=0;i<32768*5;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i;
        key->len = 4;
        key->typ = 'm';
        strcpy(key->name, "key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        kv_get(key, val);
        t = *(int*)(val->buf + 12);
        if(t!=i){
            pr_info("err %d %d\n", i, t);
        }
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
}

static void test_kv_gc_reset(void){
    struct y_key* key;
    struct y_value* val;
    int i, t;
    for(i=0;i<32768*10;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i%32768;
        key->len = 4;
        key->typ = 'm';
        strcpy(key->name, "key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        *(int*)(val->buf + 12) = i;
        kv_set(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
    for(i=0;i<32768;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i;
        key->len = 4;
        key->typ = 'm';
        strcpy(key->name, "key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        kv_get(key, val);
        t = *(int*)(val->buf + 12);
        if(t!=(32768*9+i)){
            pr_info("err %d %d\n", (32768*9+i), t);
        }
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
}

static void test_kv_lsm_flush(void){
    struct y_key* key;
    struct y_value* val;
    int i, t;
    for(i=0;i<65536*10;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i%65536;
        key->len = 24;
        key->typ = 'm';
        strcpy(key->name, "key1key1key1key1key1key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        *(int*)(val->buf + 12) = i;
        kv_set(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
    for(i=0;i<65536;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i;
        key->len = 24;
        key->typ = 'm';
        strcpy(key->name, "key1key1key1key1key1key1");
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        kv_get(key, val);
        t = *(int*)(val->buf + 12);
        if(t!=(65536*9+i)){
            pr_info("err %d %d\n", (65536*9+i), t);
        }
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
}

static void test_min_heap_block(void){
    struct min_heap h;
    unsigned int tno[10] = {0, 5, 1, 3, 6, 8, 2, 4, 2, 2};
    int i;
    unsigned int bno[10] = {3, 10, 6, 8, 4, 5, 1, 9, 7, 2};
    min_heap_init(&h, 1024, sizeof(struct y_block), y_block_greater, y_block_swap);
    h.arr = kvmalloc(h.cap*h.size, GFP_KERNEL);
    for(i=0;i<10;++i){
        struct y_block blk = {
            .table_no = tno[i],
            .block_no = bno[i],
        };
        min_heap_push(&h, &blk);
    }
    for(i=0;i<10;++i){
        struct y_block blk =* (struct y_block*)min_heap_min(&h);
        min_heap_pop(&h);
        pr_info("tno: %u, bno: %u\n", blk.table_no, blk.block_no);
    }
    kvfree(h.arr);
}

void test_rb_index_lower_bound(void){
    struct rb_root root = RB_ROOT;
    struct rb_node *cur;
    struct y_rb_index* rbi;
    struct y_key key;
    int i;
    int idx[10] = {0, 6, 4, 7, 5, 9, 4, 3, 4, 10};
    for(i=0;i<10;++i){
        rbi = kzalloc(sizeof(struct y_rb_index), GFP_KERNEL);
        rbi->start.ino = idx[i];
        rbi->blk.table_no = 0;
        rbi->blk.block_no = i;
        y_rbi_insert(&root, rbi);
    }
    for(cur=rb_first(&root); cur; cur=rb_next(cur)){
        rbi = rb_entry(cur, struct y_rb_index, node);
        pr_info("ino=%u blk=%u\n", rbi->start.ino, rbi->blk.block_no);
    }
    memset(&key, 0, sizeof(key));
    for(i=0;i<=10;++i){
        key.ino = i;
        rbi = y_rbi_lower_bound(&root, &key);
        if(rbi!=NULL){
            pr_info("lower_bound of %u: (%u, %u)\n", key.ino, rbi->start.ino, rbi->blk.block_no);
        }
        rbi = y_rbi_upper_bound(&root, &key);
        if(rbi!=NULL){
            pr_info("upper_bound of %u: (%u, %u)\n", key.ino, rbi->start.ino, rbi->blk.block_no);
        }
        if(i==4){
            for(cur = rb_prev(&rbi->node); cur; cur=rb_prev(cur)){
                rbi = rb_entry(cur, struct y_rb_index, node);
                pr_info("ino=%u blk=%u\n", rbi->start.ino, rbi->blk.block_no);
            }
        }
    }
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
    kv_init();

    pr_info("YSSD init\n");

    // test_y_rbkv_insert();
    // test_y_rbkv_update();
    // test_hash();

    // test_kv();
    // test_dump();
    // test_kv_flush();
    // test_kv_flush_get();
    // test_kv_gc();
    // test_kv_gc_reset();
    test_kv_lsm_flush();
    // test_min_heap_block();
    // test_rb_index_lower_bound();

    pr_info("YSSD test finished\n");
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