#include "kv.h"
#include "lsmtree.h"
#include "types.h"
#include "value_log.h"

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

    lt.last_k2v_page_no = OBJECT_VAL_UNFLUSH;
    vlog.lt = &lt;
    vlog.head = vlog.tail1 = vlog.tail2 = n_pages-1;
    wake_up_process(vlog.write_thread);
}

void kv_get(struct y_key* key, struct y_value* val){
    struct y_val_ptr ptr;
    mutex_lock(&glk);
    ptr = lsm_tree_get(&lt, key);
    if(key->ino==0){
        pr_info("[kv_get] (page_no, off) = (%u, %u)\n", ptr.page_no, ptr.off);
    }
    vlog_get(&vlog, key, ptr, val);
    mutex_unlock(&glk);
}

void kv_set(struct y_key* key, struct y_value* val){
    unsigned long ts = ktime_get_real_fast_ns();
    if(key->ino==0){
        pr_info("[kv_set] val=%d\tts=%lu\n", *(int*)(val->buf+12), ts);
    }
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

void mannual_gc(void){
    vlog_gc(&vlog);
}