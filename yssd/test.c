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
#include "yssd.h"

extern char g_buf[PAGE_SIZE+1];

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
    int k = 3;
    for(i=0;i<65536*k;++i){
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
        if(t!=(65536*(k-1)+i)){
            pr_info("err %d %d\n", (65536*(k-1)+i), t);
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
        struct y_block blk = *(struct y_block*)min_heap_min(&h);
        min_heap_pop(&h);
        pr_info("tno: %u, bno: %u\n", blk.table_no, blk.block_no);
        if(i==7){
            break;
        }
    }

    min_heap_clear(&h);
    for(i=7;i<10;++i){
        struct y_block blk = {
            .table_no = tno[i],
            .block_no = bno[i],
        };
        min_heap_push(&h, &blk);
    }
    while(!min_heap_empty(&h)){
        struct y_block blk = *(struct y_block*)min_heap_min(&h);
        min_heap_pop(&h);
        pr_info("[2] tno: %u, bno: %u\n", blk.table_no, blk.block_no);
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

extern struct lsm_tree lt;

static void test_lsm_flush(void){
    struct y_key* key;
    struct y_val_ptr ptr;
    int i, t;
    int k = 5;
    for(i=0;i<65536*k;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        key->ino = i%65536;
        key->len = 24;
        key->typ = 'm';
        ptr.page_no = i;
        strcpy(key->name, "key1key1key1key1key1key1");
        lsm_tree_set(&lt, key, ptr, ktime_get_real_fast_ns());
        kfree(key);
    }
    for(i=0;i<65536;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        key->ino = i;
        key->len = 24;
        key->typ = 'm';
        ptr.page_no = i;
        strcpy(key->name, "key1key1key1key1key1key1");
        ptr = lsm_tree_get(&lt, key);
        if(ptr.page_no!=65536*(k-1)+i){
            pr_err("[err] %u %u\n", ptr.page_no, 65536*(k-1)+i);
        }
        kfree(key);
    }
}

static void test_kv_iter(void){
    struct y_key* key;
    struct y_value* val;
    int i, t;
    int k = 2;
    for(i=0;i<10;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = 1234;
        key->typ = 'm';
        sprintf(key->name, "key%u", i);
        key->len = strlen(key->name);
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        *(int*)(val->buf + 12) = i;
        kv_set(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
    for(i=0;i<65536*k;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = i%65536;
        key->len = 24;
        key->typ = 'm';
        sprintf(key->name, "key%u", i);
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        *(int*)(val->buf + 12) = i;
        kv_set(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
    for(i=0;i<10;++i){
        key = kmalloc(sizeof(struct y_key), GFP_KERNEL);
        val = kmalloc(sizeof(struct y_value), GFP_KERNEL);
        val->buf = kmalloc(256, GFP_KERNEL);
        key->ino = 1234;
        key->typ = 'm';
        sprintf(key->name, "key%u", i);
        key->len = strlen(key->name);
        val->len = 256;
        memcpy(val->buf, "hello, yssd.", 12);
        *(int*)(val->buf + 12) = i;
        kv_set(key, val);
        kfree(key);
        kfree(val->buf);
        kfree(val);
    }
    // kv_iter('m', 1234, 10+k);
}