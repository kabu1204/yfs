#ifndef YSSD_LSMTREE_H
#define YSSD_LSMTREE_H
#include "rbkv.h"
#include "skiplist.h"

struct lsm_tree {
    unsigned int last_k2v_page_no;
    unsigned int last_val_page_no;

    struct rb_root mem_table;
    struct skip_list *skip_list;
};

#endif