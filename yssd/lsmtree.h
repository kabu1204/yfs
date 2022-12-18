#ifndef YSSD_LSMTREE_H
#define YSSD_LSMTREE_H
#include "bloom_filter.h"
#include "rbkv.h"
#include "skiplist.h"
#include "types.h"

#define LSM_TREE_FLUSH_PER_COMPACT 4
#define LSM_TREE_MAX_K2V_SIZE      256
#define LSM_TREE_RESET_IN_MEM_SIZE 0

struct lsm_tree {
    unsigned int head;
    unsigned int mem_size;
    unsigned int imm_size;
    unsigned int n_flush;
    unsigned int max_k2v_size;

    struct rb_root* mem_table;
    struct rb_root* imm_table;
    rwlock_t ext_lk, mem_lk, imm_lk;

    struct task_struct* compact_thread;
    wait_queue_head_t waitq;

    struct kmem_cache* rb_node_slab;
    char *comp_buf;
};

/*
    The maximum padding space for each block is (max_k2v_size-1).
    The worst case is each datablock will waste (max_k2v_size-1) bytes for padding.
*/
#define exceed_table_size(memsize, new_k2v_size, max_k2v_size) (memsize+new_k2v_size+(max(new_k2v_size, max_k2v_size)-1)*Y_DATA_BLOCK_PER_TABLE>Y_TABLE_DATA_SIZE)

void lsm_tree_init(struct lsm_tree* lt);

/*
    Can be called from access thread.
*/
struct y_val_ptr lsm_tree_get(struct lsm_tree* lt, struct y_key* key);

/*
    Can be called from access thread.
*/
void lsm_tree_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp);

/*
    Can be called from access thread and vlog write thread.
*/
int lsm_tree_get_and_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp);

void lsm_tree_del(struct lsm_tree* lt, struct y_key* key, unsigned long timestamp);

void lsm_tree_iter(struct lsm_tree* lt, struct y_key* key);

unsigned int lsm_k2v_size(struct y_key* key);

void wakeup_compact(struct lsm_tree* lt);

void memtable_flush(struct lsm_tree* lt);

void compact(struct lsm_tree* lt);

int compact_deamon(void* arg);

unsigned int dump_k2v(char* buf, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp);

#endif