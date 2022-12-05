#include "value_log.h"
#include "linux/slab.h"
#include "linux/hashtable.h"
#include "types.h"
#include "phys_io.h"
#include "linux/wait.h"
#include "linux/kthread.h"
#include "linux/sched.h"
#include "linux/completion.h"

struct value_log* vlog_create(void){
    struct value_log* vlog = kmalloc(sizeof(struct value_log), GFP_KERNEL);
    vlog->active = kzalloc(sizeof(struct hash_table), GFP_KERNEL);
    vlog->inactive = NULL;
    init_waitqueue_head(&vlog->waitq);
    vlog->write_thread = kthread_create(write_deamon, vlog, "yssd write thread");
    return vlog;
}

/*
    There's no need to consider fragmenation problem here.
    Small variable-length values will be packed into the key.
    The most common kinds of values here are: (1) >4KB (2) 256B (inode info)
*/
int vlog_append(struct value_log* vlog, struct y_k2v* k2v, struct y_value* val){
    struct vlog_node* cur;
    unsigned int v_entry_size;
    unsigned long hash;

    v_entry_size = vlog_dump_size(k2v, val);

    if(v_entry_size+vlog->in_mem_size > Y_VLOG_FLUSH_SIZE){
        vlog_wakeup_or_block(vlog);
        // vlog_flush(vlog);
    }

    hash = y_key_hash(&k2v->key);

    hash_for_each_possible(vlog->active->ht, cur, node, hash){
        if(y_key_cmp(&cur->k2v->key, &k2v->key)==0){
            cur->v = val;
            goto out;
        }
    }

    /* not exist in hash table */
    cur = kmalloc(sizeof(struct vlog_node), GFP_KERNEL);
    cur->k2v = k2v;
    cur->v = val;   // val is in ssd memory
    hash_add(vlog->active->ht, &cur->node, hash);
    vlog->in_mem_size += v_entry_size;
out:
    k2v->ptr.page_no = OBJECT_VAL_UNFLUSH;
    return 1;
}

struct y_value* vlog_get(struct value_log* vlog, struct y_k2v* k2v){
    unsigned long hash;
    struct y_value* val;
    struct vlog_node* cur;
    hash = y_key_hash(&k2v->key);

    hash_for_each_possible(vlog->active->ht, cur, node, hash){
        if(y_key_cmp(&cur->k2v->key, &k2v->key)==0){
            return cur->v;
        }
    }

    
    return NULL;
}

unsigned long vlog_dump_size(struct y_k2v* k2v, struct y_value* val){
    unsigned long res = 9; // typ(1) + ino(4) + val_len(4)
    res += val->len;
    if(k2v->key.typ==Y_KV_META) res += 1 + k2v->key.len;
    else res += 4;
    return res;
}

unsigned long vlog_node_dump(struct vlog_node* vnode, char *buf){
    unsigned long p = 0;
    buf[p++] = vnode->k2v->key.typ;
    *(unsigned int*)(buf + p) = vnode->k2v->key.ino;
    if(vnode->k2v->key.typ==Y_KV_META){
        *(char*)(buf + p + 4) = (char)(vnode->k2v->key.len);
        p += 5;
        memcpy(buf+p, vnode->k2v->key.name, vnode->k2v->key.len);
        p += vnode->k2v->key.len;
    } else {
        *(unsigned int*)(buf + p + 4) = vnode->k2v->key.len;
        p += 8;
    }
    *(unsigned int*)(buf + p) = vnode->v->len;
    p += 4;
    memcpy(buf+p, vnode->v->buf, vnode->v->len);
    p += vnode->v->len;
    return p;
}

void vlog_flush(struct value_log* vlog){
    unsigned int bkt;
    unsigned int p=0;
    unsigned long buf_size;
    unsigned int tail;
    int i;
    struct vlog_node* vnode;
    struct hlist_node* tmp;

    buf_size = vlog->in_mem_size;
    if(buf_size & (Y_PAGE_SIZE-1))
        buf_size = (buf_size + (Y_PAGE_SIZE-1)) & ~(size_t)(Y_PAGE_SIZE-1);

    char *buf = kmalloc(buf_size, GFP_KERNEL);
    hash_for_each(vlog->inactive->ht, bkt, vnode, node){
        vnode->offset = p;
        p += vlog_node_dump(vnode, buf+p);
    }

    // TODO: should get a lock here
    for(i=0; i<(buf_size/Y_PAGE_SIZE); ++i){
        yssd_write_phys_page(buf+i*Y_PAGE_SIZE, vlog->tail1+i);
    }

    tail = vlog->tail1;
    vlog->tail1 += buf_size/Y_PAGE_SIZE;

    hash_for_each_safe(vlog->inactive->ht, bkt, tmp, vnode, node){
        vnode->k2v->ptr.page_no = tail + (vnode->offset >> Y_PAGE_SHIFT);
        vnode->k2v->ptr.off = vnode->offset & (Y_PAGE_SHIFT-1);
        hash_del(&vnode->node);

        kfree(vnode->v->buf);
        kfree(vnode->v);
        kfree(vnode);
    }
}

void vlog_wakeup_or_block(struct value_log* vlog){
    wait_event_interruptible(vlog->waitq, vlog->inactive==NULL);
    vlog->inactive = vlog->active;
    vlog->active = kzalloc(sizeof(struct hash_table), GFP_KERNEL);
    wake_up_interruptible(&vlog->waitq);
}

static int write_deamon(void* arg)
{
    struct value_log* vlog = arg;
    while(1){
        wait_event_interruptible(vlog->waitq, vlog->inactive!=NULL);
        vlog_flush(vlog);
        kzfree(vlog->inactive);
        vlog->inactive = NULL;
        vlog->n_flush++;
        if(vlog->n_flush % 5 == 0){
            vlog_gc(vlog);
        }
        wake_up_interruptible(&vlog->waitq);
    }
    return 0;
}