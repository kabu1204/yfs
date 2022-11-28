#ifndef YSSD_RBKV_H
#define YSSD_RBKV_H

#include <linux/rbtree.h>
#include "types.h"

struct y_rb_node {
    struct rb_node node;
    struct y_key key;
};

struct y_rb_node* y_rb_find(struct rb_root* root, struct y_key* key);

int y_rb_insert(struct rb_root* root, struct y_rb_node* elem);

#endif