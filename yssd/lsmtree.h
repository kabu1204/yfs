#ifndef YSSD_LSMTREE_H
#define YSSD_LSMTREE_H
#include "rbkv.h"
#include "skiplist.h"
#include "types.h"

struct lsm_tree {
    unsigned int last_k2v_page_no;
    unsigned int last_val_page_no;
    unsigned long mem_table_size_bytes;

    struct rb_root mem_table;
    struct rb_root immutable;
};

struct lsm_tree* lsm_tree_create(void);

struct y_val_ptr lsm_tree_get(struct lsm_tree* lt, struct y_key* key);

void lsm_tree_set(struct lsm_tree* lt, struct y_k2v* k2v);

void lsm_tree_del(struct lsm_tree* lt, struct y_key* key);

void lsm_tree_iter(struct lsm_tree* lt, struct y_key* key);

#endif