#ifndef YSSD_LSMTREE_H
#define YSSD_LSMTREE_H
#include "rbkv.h"
#include "skiplist.h"
#include "types.h"

struct lsm_tree {
    unsigned int last_k2v_page_no;
    unsigned long mem_table_size_bytes;

    struct rb_root mem_table;
    struct rb_root immutable;
    rwlock_t lk;

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

#endif