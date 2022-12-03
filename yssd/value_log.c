#include "value_log.h"
#include "linux/slab.h"
#include "linux/hashtable.h"
#include "types.h"

struct value_log* vlog_create(void){
    struct value_log* vlog = kzalloc(sizeof(struct value_log), GFP_KERNEL);
    return vlog;
}

/*
    There's no need to consider fragmenation problem here.
    Small variable-length values will be packed into the key.
    The most common kinds of values here are: (1) >4KB (2) 256B (inode info)
*/
int vlog_append(struct value_log* vlog, struct y_k2v* k2v, struct y_value* val){
    struct vlog_node* vnode = kmalloc(sizeof(struct vlog_node), GFP_KERNEL);
    unsigned int v_entry_size;
    vnode->k2v = k2v;
    vnode->v = val; // val is in SSD memory

    v_entry_size = vlog_dump_size(vnode);

    if(v_entry_size+vlog->in_mem_size > Y_VLOG_FLUSH_SIZE){
        vlog_flush(vlog);
    }

    hash_add(vlog->ht, &vnode->node, (unsigned long)k2v);
    k2v->ptr.page_no = OBJECT_VAL_UNFLUSH;
    return 1;
}

unsigned long vlog_dump_size(struct vlog_node* vnode){
    unsigned long res=9; // typ(1) + ino(4) + val_len(4)
    res += vnode->v->len;
    if(vnode->k2v->key.typ==Y_KV_META) res += 1 + vnode->k2v->key.len;
    else res += 4;
    return res;
}

void vlog_flush(struct value_log* vlog){
    
}