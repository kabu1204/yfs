#ifndef YSSD_LSMTREE_H
#define YSSD_LSMTREE_H
#include "rbkv.h"
#include "skiplist.h"
#include "types.h"

#define LSM_TREE_FLUSH_PER_COMPACT 4
#define LSM_TREE_MAX_K2V_SIZE      256
#define LSM_TREE_RESET_IN_MEM_SIZE 0

struct lsm_tree {
    unsigned int last_k2v_page_no;
    unsigned int mem_size;
    unsigned int imm_size;
    unsigned int n_flush;
    unsigned int max_k2v_size;

    struct rb_root* mem_table;
    struct rb_root* imm_table;
    rwlock_t lk;

    struct task_struct* compact_thread;
    wait_queue_head_t waitq;

    struct kmem_cache* rb_node_slab;
};

void lsm_tree_init(struct lsm_tree* lt);

/*
    Can be called from access thread.
*/
struct y_val_ptr lsm_tree_get(struct lsm_tree* lt, struct y_key* key);

/*
    Can be called from access thread and vlog write thread.
*/
void lsm_tree_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp);

int lsm_tree_get_and_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp);

void lsm_tree_del(struct lsm_tree* lt, struct y_key* key, unsigned long timestamp);

void lsm_tree_iter(struct lsm_tree* lt, struct y_key* key);

inline unsigned int lsm_k2v_size(struct y_key* key);

void wakeup_compact(struct lsm_tree* lt);

void memtable_flush(struct lsm_tree* lt);

void compact(struct lsm_tree* lt);

int compact_deamon(void* arg);

#endif