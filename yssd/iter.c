#include "lsmtree.h"
#include "types.h"
#include "phys_io.h"
#include "mem_index.h"
#include <linux/slab.h>
#include <linux/mm.h>

inline int y_block_less(const void* left, const void* right){
    struct y_block l = *(struct y_block*)left;
    struct y_block r = *(struct y_block*)right;
    char a = l.table_no>=LSM_TREE_FLUSH_PER_COMPACT;
    char b = r.table_no<LSM_TREE_FLUSH_PER_COMPACT;
    if(a && b) return 1;
    if(!a && !b) return 0;
    if(l.table_no!=r.table_no) return l.table_no<r.table_no;
    return l.block_no<r.block_no;
}

struct y_k2v* lsm_tree_get_slow_upper_bound(struct lsm_tree* lt, struct y_key* key){
    struct y_k2v* k2v;
    struct y_key* maxk, *mink;
    struct y_rb_index *lo, *rbi;
    struct rb_node* cur;
    struct min_heap* h = &lt->hp;
    struct y_block blk;
    struct y_val_ptr ptr;
    unsigned int p = 0, size;
    char* buf;

    maxk = kmalloc(sizeof(struct y_key), GFP_KERNEL);
    maxk->typ = key->typ;
    maxk->ino = key->ino+1;
    maxk->len = 0;
    maxk->name[0]='\0';

    mink = kmalloc(sizeof(struct y_key), GFP_KERNEL);
    *mink = *maxk;

    ptr.page_no = OBJECT_NOT_FOUND;

    mutex_lock(&lt->hp_lk);
    h->less = y_block_less;
    buf = lt->read_buf;
    k2v = kzalloc(sizeof(struct y_k2v), GFP_KERNEL);

    if(unlikely(lt->mem_index_nr>h->cap)){
        kvfree(h->arr);
        h->cap <<= 1;
        h->arr = kvmalloc(h->cap*h->size, GFP_KERNEL);
    }

    read_lock(&lt->index_lk);
    lo = y_rbi_lower_bound(lt->mem_index, maxk);
    cur = (lo) ? rb_prev(&lo->node) : rb_last(lt->mem_index);
    for(; cur; cur = rb_prev(cur)){
        rbi = rb_entry(cur, struct y_rb_index, node);
        if(y_key_cmp(&rbi->end, key)>0){
            min_heap_push(h, &rbi->blk);
        }
    }

    /*
        从旧block开始扫描。每个block，遇到第一个比key大的就停下，如果比mink小，那替换mink，否则什么也不做；
    */
    while(!min_heap_empty(h)){
        blk = *(struct y_block*)min_heap_min(h);
        min_heap_pop(h);

        yssd_read_phys_pages(buf, LSM_TREE_LEVEL0_START_PAGE+blk.table_no*(Y_TABLE_SIZE>>Y_PAGE_SHIFT)+(blk.block_no+1)*(Y_BLOCK_SIZE>>Y_PAGE_SHIFT), Y_BLOCK_SIZE>>Y_PAGE_SHIFT);

        p = 0;
        while(p<Y_BLOCK_SIZE && (size = read_k2v(buf+p, k2v))!=0){
            if(unlikely(y_key_cmp(&k2v->key, maxk)>=0)){
                break;
            }
            if(y_key_cmp(&k2v->key, key)>0){
                // TODO
                if(y_key_cmp(&k2v->key, mink)<=0){
                    *mink = k2v->key;
                    ptr = k2v->ptr;
                }
                break;
            }
            p += size;
        }

    }
out:
    min_heap_clear(h);
    h->less = y_block_greater;
    read_unlock(&lt->index_lk);
    mutex_unlock(&lt->hp_lk);

    k2v->key = *mink;
    k2v->ptr = ptr;

    kfree(mink);
    kfree(maxk);

    return k2v;
}