#include "bloom_filter.h"
#include "rbkv.h"
#include "types.h"
#include "lsmtree.h"
#include "linux/slab.h"
#include "value_log.h"
#include <linux/mm.h>
#include <linux/wait.h>

unsigned int counter=0;

void lsm_tree_init(struct lsm_tree* lt){
    lt->mem_table = kzalloc(sizeof(struct rb_root), GFP_KERNEL);
    lt->imm_table = NULL;
    lt->n_flush = 0;
    lt->max_k2v_size = 0;
    lt->mem_size = LSM_TREE_RESET_IN_MEM_SIZE;
    lt->comp_buf = kvmalloc(Y_TABLE_SIZE, GFP_KERNEL);
    init_waitqueue_head(&lt->waitq);
    lt->compact_thread = kthread_create(compact_deamon, lt, "yssd compact thread");
    lt->rb_node_slab = kmem_cache_create("yssd_lsm_tree_rb_node", sizeof(struct y_rb_node), 0, SLAB_HWCACHE_ALIGN, NULL);
    rwlock_init(&lt->mem_lk);
    rwlock_init(&lt->imm_lk);
    rwlock_init(&lt->ext_lk);

    pr_info("[lsmtree] Y_BLOCK_SIZE: %luB(%luKB)\n", Y_BLOCK_SIZE, Y_BLOCK_SIZE>>10);
    pr_info("[lsmtree] Y_TABLE_SIZE: %luB(%luKB)\n", Y_TABLE_SIZE, Y_TABLE_SIZE>>10);
    pr_info("[lsmtree] Y_BLOOM_SIZE_BYTES: %uB\n", Y_BLOOM_SIZE_BYTES);
    pr_info("[lsmtree] Y_DATA_BLOCK_PER_TABLE: %u\n", Y_DATA_BLOCK_PER_TABLE);
    pr_info("[lsmtree] Y_TABLE_DATA_SIZE: %uKB\n", Y_TABLE_DATA_SIZE>>10);
}

struct y_val_ptr lsm_tree_get(struct lsm_tree* lt, struct y_key* key){
    struct y_rb_node* node;
    struct y_val_ptr ptr;
    read_lock(&lt->ext_lk);

    read_lock(&lt->mem_lk);
    node = y_rb_find(lt->mem_table, key);
    if(likely(node)){
        read_unlock(&lt->mem_lk);
        read_unlock(&lt->ext_lk);
        return node->kv.ptr;
    }
    read_unlock(&lt->mem_lk);
    read_lock(&lt->imm_lk);
    if(!lt->imm_table){
        read_unlock(&lt->imm_lk);
        goto slow;
    }
    node = y_rb_find(lt->imm_table, key);
    if(likely(node)){
        read_unlock(&lt->imm_lk);
        read_unlock(&lt->ext_lk);
        return node->kv.ptr;
    }
    read_unlock(&lt->imm_lk);
    
slow:
    pr_info("[lsmtree] search in disk\n");
    // TODO: search in disk

    read_unlock(&lt->ext_lk);
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

    write_lock(&lt->ext_lk);
    if(counter % 4096*3 == 0){
        pr_info("[lsmtree] mem_size: %uKB\n", lt->mem_size>>10);
    }
    ++counter;
    if(unlikely(exceed_table_size(lt->mem_size, k2v_size, lt->max_k2v_size))){
        wakeup_compact(lt);
    }
    
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr = ptr;
    node->kv.timestamp = timestamp;

    write_lock(&lt->mem_lk);
    res = y_rb_insert(lt->mem_table, node);
    if(unlikely(res==-1)){
        pr_warn("invalid timestamp\n");
    }
    if(res==0){ // update
        kmem_cache_free(lt->rb_node_slab, node);
    } else {    // newly insert
        lt->mem_size += k2v_size;
    }
    lt->max_k2v_size = max(k2v_size, lt->max_k2v_size);
    write_unlock(&lt->mem_lk);

    write_unlock(&lt->ext_lk);
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

    write_lock(&lt->ext_lk);
    if(counter % 4096*3 == 0){
        pr_info("[lsmtree] mem_size: %uKB\n", lt->mem_size>>10);
    }
    ++counter;
    if(unlikely(exceed_table_size(lt->mem_size, k2v_size, lt->max_k2v_size))){
        wakeup_compact(lt);
    }

    read_lock(&lt->mem_lk);
    node = y_rb_find(lt->mem_table, key);
    if(likely(node)){
        if(node->kv.timestamp > timestamp){
            // pr_info("[lsmtree] found newer record\n");
            read_unlock(&lt->mem_lk);
            write_unlock(&lt->ext_lk);
            return 0;
        }
        read_unlock(&lt->mem_lk);
        goto set;
    }
    read_unlock(&lt->mem_lk);

    read_lock(&lt->imm_lk);
    if(!lt->imm_table){
        read_unlock(&lt->imm_lk);
        goto slow;
    }
    node = y_rb_find(lt->imm_table, key);
    if(likely(node)){
        if(node->kv.timestamp > timestamp){
            pr_info("[lsmtree] found newer record\n");
            read_unlock(&lt->imm_lk);
            write_unlock(&lt->ext_lk);
            return 0;
        }
        read_unlock(&lt->imm_lk);
        goto set;
    }
    read_unlock(&lt->imm_lk);

slow:
    pr_info("[lsmtree] get_and_set search in disk\n");

set:
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr = ptr;
    node->kv.timestamp = timestamp;

    write_lock(&lt->mem_lk);
    res = y_rb_insert(lt->mem_table, node);
    if(res==-1){
        pr_warn("invalid timestamp\n");
    }
    if(res==0){ // update
        kmem_cache_free(lt->rb_node_slab, node);
    } else {    // newly insert
        lt->mem_size += k2v_size;
    }
    lt->max_k2v_size = max(k2v_size, lt->max_k2v_size);
    write_unlock(&lt->mem_lk);

    write_unlock(&lt->ext_lk);
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

    write_lock(&lt->ext_lk);
    if(unlikely(exceed_table_size(lt->mem_size, k2v_size, lt->max_k2v_size))){
        wakeup_compact(lt);
    }

    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr.page_no = OBJECT_DEL;
    node->kv.timestamp = timestamp;

    write_lock(&lt->mem_lk);
    res = y_rb_insert(lt->mem_table, node);
    if(res==-1){
        pr_warn("invalid timestamp\n");
    }
    if(res==0){ // update
        kmem_cache_free(lt->rb_node_slab, node);
    } else {    // newly insert
        lt->mem_size += k2v_size;
    }
    lt->max_k2v_size = max(k2v_size, lt->max_k2v_size);
    write_unlock(&lt->mem_lk);

    write_unlock(&lt->ext_lk);
}

void lsm_tree_iter(struct lsm_tree* lt, struct y_key* key){

}