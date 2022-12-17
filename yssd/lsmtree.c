#include "bloom_filter.h"
#include "rbkv.h"
#include "types.h"
#include "lsmtree.h"
#include "linux/slab.h"
#include "value_log.h"
#include <linux/mm.h>

void lsm_tree_init(struct lsm_tree* lt){
    lt->mem_table = kzalloc(sizeof(struct rb_root), GFP_KERNEL);
    lt->imm_table = NULL;
    lt->mem_bf = bloom_alloc();
    lt->imm_bf = NULL;
    lt->n_flush = 0;
    lt->max_k2v_size = 0;
    lt->mem_size = LSM_TREE_RESET_IN_MEM_SIZE;
    lt->comp_buf = kvmalloc(Y_TABLE_SIZE, GFP_KERNEL);
    lt->rb_node_slab = kmem_cache_create("yssd_lsm_tree_rb_node", sizeof(struct y_rb_node), 0, SLAB_HWCACHE_ALIGN, NULL);
    rwlock_init(&lt->lk);
}

struct y_val_ptr lsm_tree_get(struct lsm_tree* lt, struct y_key* key){
    // TODO: re-consider thread safety
    struct y_rb_node* node;
    struct y_val_ptr ptr;
    read_lock(&lt->lk);
    node = y_rb_find(lt->mem_table, key);
    if(likely(node)){
        read_unlock(&lt->lk);
        return node->kv.ptr;
    }
    pr_info("[lsmtree] search in disk\n");
    // TODO: search in disk
    read_unlock(&lt->lk);
    return ptr;
}

void lsm_tree_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp){
    struct y_rb_node* node;
    unsigned int k2v_size;
    int res;
    k2v_size = lsm_k2v_size(key);
    if(unlikely(k2v_size>LSM_TREE_MAX_K2V_SIZE)){
        pr_err("[lsmtree] k2v exceeds size limit(%u)", LSM_TREE_MAX_K2V_SIZE);
        return;
    }

    write_lock(&lt->lk);
    if(unlikely(exceed_table_size(lt->mem_size, k2v_size, lt->max_k2v_size))){
        wakeup_compact(lt);
    }
    
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr = ptr;
    node->kv.timestamp = timestamp;
    res = y_rb_insert(lt->mem_table, node);
    if(unlikely(res==-1)){
        pr_warn("invalid timestamp\n");
    }
    if(res==0){
        kmem_cache_free(lt->rb_node_slab, node);
    }
    lt->max_k2v_size = max(k2v_size, lt->max_k2v_size);

    write_unlock(&lt->lk);
}

int lsm_tree_get_and_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp){
    struct y_rb_node* node;
    unsigned int k2v_size;
    int res;
    k2v_size = lsm_k2v_size(key);
    if(unlikely(k2v_size>LSM_TREE_MAX_K2V_SIZE)){
        pr_err("[lsmtree] k2v exceeds size limit(%u)", LSM_TREE_MAX_K2V_SIZE);
        return -1;
    }

    write_lock(&lt->lk);
    if(unlikely(exceed_table_size(lt->mem_size, k2v_size, lt->max_k2v_size))){
        wakeup_compact(lt);
    }

    node = y_rb_find(lt->mem_table, key);
    if(likely(node)){
        if(node->kv.timestamp > timestamp){
            pr_info("[lsmtree] found newer record\n");
            write_unlock(&lt->lk);
            return 0;
        }
    } else {
        pr_info("[lsmtree] search in disk\n");
        // TODO: search in disk
    }
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr = ptr;
    node->kv.timestamp = timestamp;
    res = y_rb_insert(lt->mem_table, node);
    if(res==-1){
        pr_warn("invalid timestamp\n");
    }
    if(res==0){
        kmem_cache_free(lt->rb_node_slab, node);
    }
    lt->max_k2v_size = max(k2v_size, lt->max_k2v_size);
    write_unlock(&lt->lk);
    return 1;
}

void lsm_tree_del(struct lsm_tree* lt, struct y_key* key, unsigned long timestamp){
    struct y_rb_node* node;
    unsigned int k2v_size;
    int res;
    k2v_size = lsm_k2v_size(key);
    if(unlikely(k2v_size>LSM_TREE_MAX_K2V_SIZE)){
        pr_err("[lsmtree] k2v exceeds size limit(%u)", LSM_TREE_MAX_K2V_SIZE);
        return;
    }
    if(unlikely(exceed_table_size(lt->mem_size, k2v_size, lt->max_k2v_size))){
        wakeup_compact(lt);
    }

    write_lock(&lt->lk);
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr.page_no = OBJECT_DEL;
    node->kv.timestamp = timestamp;
    res = y_rb_insert(lt->mem_table, node);
    if(res==-1){
        pr_warn("invalid timestamp\n");
    }
    if(res==0){
        kmem_cache_free(lt->rb_node_slab, node);
    }
    lt->max_k2v_size = max(k2v_size, lt->max_k2v_size);
    write_unlock(&lt->lk);
}

void lsm_tree_iter(struct lsm_tree* lt, struct y_key* key){

}