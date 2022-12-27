#ifndef YSSD_RBKV_H
#define YSSD_RBKV_H

#include <linux/rbtree.h>
#include "types.h"

struct y_rb_node {
    struct rb_node node;
    struct y_k2v kv;
    struct y_rb_node* nxt;  // for iteration
};

struct y_rb_node* y_rb_find(struct rb_root* root, struct y_key* key);

struct y_rb_node* y_rb_upper_bound(struct rb_root* root, struct y_key* key);

int y_rb_insert(struct rb_root* root, struct y_rb_node* elem);

void test_y_rbkv_insert(void);
void test_y_rbkv_update(void);

#endif