#include "value_log.h"
#include "linux/slab.h"
#include "linux/hashtable.h"
#include "lsmtree.h"
#include "types.h"
#include "phys_io.h"
#include "linux/wait.h"
#include "linux/kthread.h"
#include "linux/sched.h"
#include "linux/completion.h"
#include "linux/delay.h"
#include "linux/mm.h"

static unsigned long counter = 0;
static unsigned long flush_cnt = 0;

extern unsigned long n_pages;

void vlog_init(struct value_log* vlog){
    vlog->in_mem_size = VLOG_RESET_IN_MEM_SIZE;
    vlog->active = kvzalloc(sizeof(struct hash_table), GFP_KERNEL);
    vlog->inactive = NULL;
    vlog->n_flush = 0;
    vlog->catchup = 0;
    init_waitqueue_head(&vlog->waitq);
    rwlock_init(&vlog->rwlock);
    rwlock_init(&vlog->act_lk);
    rwlock_init(&vlog->inact_lk);
    vlog->write_thread = kthread_create(write_deamon, vlog, "yssd write thread");
    vlog->vl_slab = kmem_cache_create("vlog_list_node_cache", sizeof(struct vlog_list_node), 0, SLAB_HWCACHE_ALIGN, NULL);
    vlog->vh_slab = kmem_cache_create("vlog_hlist_node_cache", sizeof(struct vlog_hlist_node), 0, SLAB_HWCACHE_ALIGN, NULL);

    init_gc_stat();
    pr_info("[vlog]VLOG_FLUSH_SIZE: %lu B\n", Y_VLOG_FLUSH_SIZE);
}

/*
    There's no need to consider fragmenation problem here.
    Small variable-length values will be packed into the key.
    The most common kinds of values here are: (1) >4KB (2) 256B (inode info)

    vlog_append will memcpy val->buf.

    Concurrent-UNSAFE.
*/
int vlog_append(struct value_log* vlog, struct y_key* key, struct y_value* val, unsigned long timestamp){
    struct vlog_hlist_node* cur;
    unsigned int v_entry_size;
    unsigned long hash;

    if(unlikely(key->typ!='m')){
        pr_warn("append data object\n");
    }

    v_entry_size = vlog_dump_size(key, val);

    if((counter % 4096)==0){
        pr_info("[vlog]in_mem_size: %luKB", vlog->in_mem_size/1024);
    }
    if(v_entry_size+vlog->in_mem_size > Y_VLOG_FLUSH_SIZE){
        // vlog_flush_sync(vlog);
        vlog_wakeup_or_block(vlog);
    }

    hash = y_key_hash(key);

    write_lock(&vlog->act_lk);
    hash_for_each_possible(vlog->active->ht, cur, hnode, hash){
        if(y_key_cmp(&cur->vnode.key, key)==0){
            if(unlikely(timestamp < cur->vnode.timestamp)){
                pr_warn("invalid timestamp\n"); 
                return -1;
            }
            cur->vnode.timestamp = timestamp;
            vlog->in_mem_size += val->len - cur->vnode.v.len;
            valcpy(&cur->vnode.v, val);
            write_unlock(&vlog->act_lk);
            goto out;
        }
    }
    write_unlock(&vlog->act_lk);

    /* not exist in hash table */
    cur = kmem_cache_alloc(vlog->vh_slab, GFP_KERNEL);
    cur->vnode.timestamp = timestamp;
    cur->vnode.key = *key;
    cur->vnode.v.len = val->len;
    cur->vnode.v.buf = kzalloc(val->len, GFP_KERNEL);
    memcpy(cur->vnode.v.buf, val->buf, val->len);
    write_lock(&vlog->act_lk);
    hash_add(vlog->active->ht, &cur->hnode, hash);
    write_unlock(&vlog->act_lk);
    vlog->in_mem_size += v_entry_size;
    ++counter;
out:
    return 1;
}

/*
    vlog_get() will assume the caller is holding a GET level lock,
    i.e. no concurrent SET(k2v, other value) would happen.
*/
int vlog_get(struct value_log* vlog, struct y_key* key, struct y_val_ptr ptr, struct y_value* v){
    unsigned long hash;
    struct vlog_hlist_node* cur;
    int trial;
    int npages;
    char *buf;
    hash = y_key_hash(key);

    if(ptr.page_no!=OBJECT_VAL_UNFLUSH){
        goto slow;
    }

    read_lock(&vlog->act_lk);
    hash_for_each_possible(vlog->active->ht, cur, hnode, hash){
        if(y_key_cmp(&cur->vnode.key, key)==0){
            v->len = cur->vnode.v.len;
            if(!v->buf){
                pr_warn("[vlog_get] allocating space for v->buf: %uB\n", v->len);
                v->buf = kzalloc(v->len, GFP_KERNEL);
            }
            memcpy(v->buf, cur->vnode.v.buf, v->len);
            read_unlock(&vlog->act_lk);
            return 1;
        }
    }
    read_unlock(&vlog->act_lk);

    read_lock(&vlog->inact_lk);
    if(!vlog->inactive){
        read_unlock(&vlog->inact_lk);
        goto retry;
    }
    pr_info("[vlog_get] search in inact table\n");
    hash_for_each_possible(vlog->inactive->ht, cur, hnode, hash){
        if(y_key_cmp(&cur->vnode.key, key)==0){
            v->len = cur->vnode.v.len;
            if(!v->buf){
                pr_warn("[vlog_get] allocating space for v->buf: %uB\n", v->len);
                v->buf = kzalloc(v->len, GFP_KERNEL);
            }
            memcpy(v->buf, cur->vnode.v.buf, v->len);
            read_unlock(&vlog->inact_lk);
            return 1;
        }
    }
    read_unlock(&vlog->inact_lk);

retry:
    /*
        GET(k, v):
            1. k2v <== lsmtree | k2v.UNFLUSH = true;
            2. write_thread flush the value of k2v
            3. write_thread remove value from hashtable
        So we need to re-get from lsmtree.
    */
    pr_info("[vlog_get] try to sleep and re-get\n");
    trial=3;
    while(trial--){
        msleep(5);
        ptr = lsm_tree_get(vlog->lt, key);
        if(ptr.page_no > OBJECT_VAL_UNFLUSH){
            break;
        }
    }
    if(ptr.page_no<=OBJECT_VAL_UNFLUSH){
        pr_info("[vlog_get] re-get failed\n");
        buf = kmalloc(sizeof(struct y_key)+24, GFP_KERNEL);
        sprint_y_key(buf, key);
        pr_warn("[vlog_get] incorrect page_no: %u, key=%s\n", ptr.page_no, buf);
        kfree(buf);
        return -1;
    }

slow:
    read_lock(&vlog->rwlock);
    if(unlikely(ptr.page_no<=vlog->tail1 || (ptr.page_no >= vlog->tail2 && ptr.page_no<vlog->head) || ptr.page_no>=n_pages)){
        read_unlock(&vlog->rwlock);
        buf = kmalloc(sizeof(struct y_key)+24, GFP_KERNEL);
        sprint_y_key(buf, key);
        pr_warn("[vlog_get slow] incorrect page_no: %u, key=%s\n", ptr.page_no, buf);
        kfree(buf);
        return -1;
    }
    npages = (unlikely(ptr.page_no==vlog->head || ptr.page_no==n_pages-1))?1:2;
    read_unlock(&vlog->rwlock);

    buf = kzalloc(Y_PAGE_SIZE<<(npages-1), GFP_KERNEL);
    yssd_read_phys_pages(buf, ptr.page_no, npages);
    if(unlikely(vlog_read_value(buf+ptr.off, key, v)==0)){
        sprint_y_key(buf, key);
        pr_err("[vlog_get] unmatched KV: key=%s\n", buf);
        kzfree(buf);
        return -1;
    }
    kzfree(buf);
    return 0;
}

unsigned long vlog_dump_size(struct y_key* key, struct y_value* val){
    unsigned long res = 18; // start_identifier '#'(1) + typ(1) + ino(4) + val_len(4) + timestamp(8)
    res += val->len;
    if(key->typ==Y_KV_META) res += 1 + key->len;
    else res += 4;
    return res;
}

unsigned long vlog_node_dump(struct vlog_node* vnode, char *buf){
    // 1 + 8 + 1 + 4 + 1 + klen + 4 + vlen
    unsigned long p = 1;
    buf[0] = '#';
    *(unsigned long*)(buf + p) = vnode->timestamp;
    p += 8;
    buf[p++] = vnode->key.typ;
    *(unsigned int*)(buf + p) = vnode->key.ino;
    p += 4;
    if(vnode->key.typ==Y_KV_META){
        *(char*)(buf + p) = (char)(vnode->key.len);
        p += 1;
        memcpy(buf+p, vnode->key.name, vnode->key.len);
        p += vnode->key.len;
    } else {
        *(unsigned int*)(buf + p) = vnode->key.len;
        p += 4;
    }
    *(unsigned int*)(buf + p) = vnode->v.len;
    p += 4;
    if(flush_cnt >= counter) {
        pr_info("p=%lu\n", p);
        pr_info("buf+p=%p\n", buf+p);
        pr_info("p+vlen=%lu\n", p+vnode->v.len);
    }
    memcpy(buf+p, vnode->v.buf, vnode->v.len);
    p += vnode->v.len;
    return p;
}

unsigned long vlog_node_load(char *buf, struct vlog_node* vnode){
    unsigned long p=1;
    struct y_key* key = &vnode->key;
    struct y_value* v = &vnode->v;
    if(unlikely(buf[0]!='#')) return 0;

    vnode->timestamp = *(unsigned long*)(buf+p);
    p += 8;
    key->typ = buf[p++];
    key->ino = *(unsigned int*)(buf+p);
    p += 4;
    if(key->typ=='m'){
        key->len = *(char*)(buf+p);
        ++p;
        memcpy(key->name, buf+p, key->len);
        p+=key->len;
    } else {
        key->len = *(unsigned int*)(buf+p);
        p+=4;
    }
    v->len = *(unsigned int*)(buf+p);
    p += 4;
    if(unlikely(v->buf)){
        kzfree(v->buf);
    }
    v->buf = kzalloc(v->len, GFP_KERNEL);
    memcpy(v->buf, buf+p, v->len);
    p += v->len;
    return p;
}

unsigned long vlog_read_value(char *buf, struct y_key* key, struct y_value* v){
    unsigned long p=1;
    if(unlikely(buf[0]!='#')) return 0;
    p += 8; // timestamp

    if(unlikely(key->typ != buf[p++])){
        pr_err("unmatch typ: expect %c, got %c\n", key->typ, buf[p-1]);
        return 0;
    }
    if(unlikely(key->ino != *(unsigned int*)(buf+p))){
        pr_err("unmatch ino: expect %u, got %u\n", key->ino, *(unsigned int*)(buf+p));
        return 0;
    }
    p += 4;
    if(key->typ=='m'){
        if(unlikely(key->len != *(char*)(buf+p))){
            pr_err("unmatch key->len: expect %u, got %u\n", key->len, *(char*)(buf+p));
            return 0;
        }
        ++p;
        if(unlikely(memcmp(key->name, buf+p, key->len)!=0)){
            char *tmp = kmalloc(key->len+1, GFP_KERNEL);
            tmp[key->len] = '\0';
            memcpy(tmp, buf+p, key->len);
            pr_err("unmatch name: expect %s, got %s\n", key->name, tmp);
            kfree(tmp);
            return 0;
        }
        p += key->len;
    } else {
        if(unlikely(key->len != *(unsigned int*)(buf+p))){
            pr_err("unmatch key->len: expect %u, got %u\n", key->len, *(unsigned int*)(buf+p));
            return 0;
        }
        p += 4;
    }
    v->len = *(unsigned int*)(buf+p);
    p += 4;
    if(!v->buf) v->buf = kmalloc(v->len, GFP_KERNEL);
    memcpy(v->buf, buf+p, v->len);
    p += v->len;
    return p;
}

void valcpy(struct y_value* to, const struct y_value* from){
    if(to->len < from->len){
        kzfree(to->buf);
        kzalloc(from->len, GFP_KERNEL);
    }
    to->len = from->len;
    memcpy(to->buf, from->buf, from->len);
}

void vlog_flush(struct value_log* vlog){
    unsigned int bkt;
    unsigned int p=0;
    unsigned long buf_size;
    unsigned int npages;
    unsigned int tail;
    char *buf;
    struct lsm_tree* lt;
    struct vlog_hlist_node* vhnode;
    struct hlist_node* tmp;
    struct hash_table* inact;

    lt = vlog->lt;
    buf_size = vlog->inact_size;
    buf_size = align_backward(buf_size, Y_PAGE_SHIFT);
    npages = buf_size >> Y_PAGE_SHIFT;

    pr_info("[flush] inactive hashtable size = %luB\n", vlog->inact_size);
    pr_info("[flush] buf_size = %lu\n", buf_size);
    pr_info("[flush] npages   = %u\n", npages);

    buf = vzalloc(buf_size);
    hash_for_each(vlog->inactive->ht, bkt, vhnode, hnode){
        vhnode->vnode.offset = p;
        ++flush_cnt;
        if(flush_cnt >= counter) {
            pr_info("p=%u\n", p);
            pr_info("%lu %lu\n", counter, flush_cnt);
        }
        p += vlog_node_dump(&vhnode->vnode, buf+p);
    }

    *(unsigned int*)(&buf[buf_size-4]) = npages;

    pr_info("[flush] dump to mem buffer finished\n");

    /*
        Do not need a rwlock here, because flushed 
        values are still in inactive hash table.
    */
    if(vlog->tail2 - vlog->head>=npages){
        vlog->tail2 -= npages;
        tail = vlog->tail2;
        pr_info("[flush] tail2 moved from %u to %u\n", tail+npages, tail);
    } else {
        vlog->tail1 -= npages;
        tail = vlog->tail1;
        pr_info("[flush] tail1 moved from %u to %u\n", tail+npages, tail);
    }

    yssd_write_phys_pages(buf, tail+1, npages);

    vfree(buf);

    pr_info("[flush] write to physical pages finished\n");

    write_lock(&vlog->inact_lk);
    inact = vlog->inactive;
    vlog->inactive = NULL;
    write_unlock(&vlog->inact_lk);

    hash_for_each_safe(inact->ht, bkt, tmp, vhnode, hnode){
        struct vlog_node* vnode = &vhnode->vnode;
        struct y_val_ptr ptr = {
            .page_no = tail + 1 + (vnode->offset >> Y_PAGE_SHIFT),
            .off = vnode->offset & (Y_PAGE_SIZE-1),
        };
        lsm_tree_get_and_set(lt, &vnode->key, ptr, vnode->timestamp);
        hash_del(&vhnode->hnode);

        kzfree(vnode->v.buf);
        kmem_cache_free(vlog->vh_slab, vhnode);
    }
    kvfree(inact);
    pr_info("[flush] free inactive hashtable finished\n");
}

void vlog_flush_sync(struct value_log* vlog){
    pr_info("flush triggered\n");
    vlog->inactive = vlog->active;
    vlog->active = kvzalloc(sizeof(struct hash_table), GFP_KERNEL);
    vlog->inact_size = vlog->in_mem_size;
    vlog->in_mem_size = VLOG_RESET_IN_MEM_SIZE;
    wake_up_interruptible(&vlog->waitq);
    wait_event_interruptible(vlog->waitq, vlog->inactive==NULL);
}

void vlog_wakeup_or_block(struct value_log* vlog){
    pr_info("flush triggered\n");
    wait_event_interruptible(vlog->waitq, vlog->inactive==NULL);
    vlog->inactive = vlog->active;
    vlog->active = kvzalloc(sizeof(struct hash_table), GFP_KERNEL);
    vlog->inact_size = vlog->in_mem_size;
    vlog->in_mem_size = VLOG_RESET_IN_MEM_SIZE;
    wake_up_interruptible(&vlog->waitq);
}

int write_deamon(void* arg)
{
    struct value_log* vlog = arg;
    while(1){
        wait_event_interruptible(vlog->waitq, vlog->inactive!=NULL);
        pr_info("write thread wake up\n");
        vlog_flush(vlog);
        pr_info("[flush] flush finished\n");
        ++vlog->n_flush;
        if(vlog->n_flush % VLOG_N_FLUSH_PER_GC == 0){
            vlog->catchup = 0;
            pr_info("[GC] GC triggered\n");
            vlog_gc(vlog);
            pr_info("[GC] finished\n");
        } else {
            gc_early(vlog);
        }
        wake_up_interruptible(&vlog->waitq);
    }
    return 0;
}