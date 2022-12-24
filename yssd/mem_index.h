#ifndef YSSD_MEM_INDEX_H
#define YSSD_MEM_INDEX_H

#include <linux/rbtree.h>
#include "types.h"
#include <linux/hashtable.h>

struct y_block {
    unsigned int table_no;
    unsigned int block_no;
};

struct y_rb_index {
    struct rb_node node;
    struct y_key start;
    struct y_key end;
    struct y_block blk;
    struct y_rb_index* nxt;  // for iteration
};

struct mem_index {
    struct rb_root rb;
};

int y_block_greater(const void* left, const void* right);

void y_block_swap(void* left, void *right);

int y_rbi_cmp(struct y_rb_index* left, struct y_rb_index* right);

struct y_rb_index* y_rbi_find(struct rb_root* root, struct y_rb_index* target);

int y_rbi_insert(struct rb_root* root, struct y_rb_index* elem);

struct y_rb_index* y_rbi_lower_bound(struct rb_root* root, struct y_key* key);

struct y_rb_index* y_rbi_upper_bound(struct rb_root* root, struct y_key* key);

#endif