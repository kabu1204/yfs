#include "kv.h"
#include "lsmtree.h"
#include "types.h"
#include "value_log.h"
#include "yssd.h"

extern unsigned long n_pages;

struct lsm_tree lt;
struct value_log vlog;
struct mutex glk;

struct y_val_ptr unflush_ptr = {
    .page_no = OBJECT_VAL_UNFLUSH
};

void kv_init(){
    lsm_tree_init(&lt);
    vlog_init(&vlog);
    mutex_init(&glk);

    vlog.lt = &lt;
    vlog.head = vlog.tail1 = vlog.tail2 = n_pages-1;
    wake_up_process(vlog.write_thread);
    wake_up_process(lt.compact_thread);
}

int kv_get(struct y_key* key, struct y_value* val){
    struct y_val_ptr ptr;
    mutex_lock(&glk);
    ptr = lsm_tree_get(&lt, key);
    if(ptr.page_no==OBJECT_DEL || ptr.page_no==OBJECT_NOT_FOUND){
        mutex_unlock(&glk);
        return ((ptr.page_no==OBJECT_DEL) ? ERR_DELETED : ERR_NOT_FOUND);
    }
    // if(ptr.page_no)
    vlog_get(&vlog, key, ptr, val);
    mutex_unlock(&glk);
    return 0;
}

void kv_set(struct y_key* key, struct y_value* val){
    unsigned long ts = ktime_get_real_fast_ns();
    mutex_lock(&glk);
    if(unlikely(vlog_append(&vlog, key, val, ts)<0)){
        pr_err("kv_set failed\n");
        return;
    }
    /*
        flush of this value would not happen. Flush of current val won't happen 
        until next value is appended to trigger flush. However, we are holding the
        mutex, no other SET can happen.
    */
    lsm_tree_set(&lt, key, unflush_ptr, ts);
    mutex_unlock(&glk);
}

void kv_del(struct y_key* key){
    unsigned long ts = ktime_get_real_fast_ns();
    mutex_lock(&glk);
    lsm_tree_del(&lt, key, ts);
    mutex_unlock(&glk);
}

int kv_iter(char typ, unsigned int ino, struct y_key* key, struct y_value* val){
    struct y_k2v* k2v;
    int res = 0;

    key->typ = typ;
    key->ino = ino;
    key->len = 0;
    key->name[0] = '\0';

    mutex_lock(&glk);

    k2v = lsm_tree_get_upper_bound(&lt, key);

    if(k2v->ptr.page_no==OBJECT_DEL || k2v->ptr.page_no==OBJECT_NOT_FOUND){
        res = ((k2v->ptr.page_no==OBJECT_DEL) ? ERR_DELETED : ERR_NOT_FOUND);
        goto out;
    }
    
    vlog_get(&vlog, &k2v->key, k2v->ptr, val);

out:
    mutex_unlock(&glk);
    *key = k2v->key;
    kfree(k2v);
    return res;
}

int kv_next(struct y_key* key, struct y_value* val){
    struct y_k2v* k2v = NULL;
    int res = 0;
    mutex_lock(&glk);
    k2v = lsm_tree_get_upper_bound(&lt, key);

    if(k2v->ptr.page_no==OBJECT_DEL || k2v->ptr.page_no==OBJECT_NOT_FOUND){
        res = ((k2v->ptr.page_no==OBJECT_DEL) ? ERR_DELETED : ERR_NOT_FOUND);
        goto out;
    }

    vlog_get(&vlog, &k2v->key, k2v->ptr, val);
out:
    mutex_unlock(&glk);
    *key = k2v->key;
    kfree(k2v);
    return res;
}

void mannual_gc(void){
    vlog_gc(&vlog);
}

void kv_close(void){
    mutex_lock(&glk);
    vlog_close(&vlog);
    lsm_tree_close(&lt);
    mutex_unlock(&glk);
    mutex_destroy(&glk);
}