#include "rbkv.h"
#include "types.h"
#include "lsmtree.h"
#include "linux/slab.h"
#include "value_log.h"

void lsm_tree_init(struct lsm_tree* lt){
    lt->mem_table = RB_ROOT;
    lt->rb_node_slab = kmem_cache_create("yssd_lsm_tree_rb_node", sizeof(struct y_rb_node), 0, SLAB_HWCACHE_ALIGN, NULL);
    rwlock_init(&lt->lk);
}

struct y_val_ptr lsm_tree_get(struct lsm_tree* lt, struct y_key* key){
    // TODO: re-consider thread safety
    struct y_rb_node* node;
    struct y_val_ptr ptr;
    read_lock(&lt->lk);
    node = y_rb_find(&lt->mem_table, key);
    if(likely(node)){
        read_unlock(&lt->lk);
        return node->kv.ptr;
    }
    // TODO: search in disk
    read_unlock(&lt->lk);
    return ptr;
}

void lsm_tree_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp){
    struct y_rb_node* node;
    write_lock(&lt->lk);
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr = ptr;
    node->kv.timestamp = timestamp;
    if(y_rb_insert(&lt->mem_table, node)==-1){
        pr_warn("invalid timestamp\n");
    }
    write_unlock(&lt->lk);
}

int lsm_tree_get_and_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp){
    struct y_rb_node* node;
    write_lock(&lt->lk);
    node = y_rb_find(&lt->mem_table, key);
    if(likely(node)){
        if(node->kv.timestamp > timestamp){
            write_unlock(&lt->lk);
            return 0;
        }
    } else {
        // TODO: search in disk
    }
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr = ptr;
    node->kv.timestamp = timestamp;
    if(y_rb_insert(&lt->mem_table, node)==-1){
        pr_warn("invalid timestamp\n");
    }

    write_unlock(&lt->lk);
    return 1;
}

void lsm_tree_del(struct lsm_tree* lt, struct y_key* key, unsigned long timestamp){
    struct y_rb_node* node;
    write_lock(&lt->lk);
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr.page_no = OBJECT_DEL;
    node->kv.timestamp = timestamp;
    if(y_rb_insert(&lt->mem_table, node)==-1){
        pr_warn("invalid timestamp\n");
    }
    write_unlock(&lt->lk);
}

void lsm_tree_iter(struct lsm_tree* lt, struct y_key* key){

}