#ifndef YSSD_SKIPLIST_H
#define YSSD_SKIPLIST_H

#include "types.h"
#define SKIP_LIST_MAX_LVL 32

struct skip_node {
    struct skip_node* next[SKIP_LIST_MAX_LVL];
};

struct skip_list {
    struct skip_node* head;
    int lvl;
    int size;
};

struct y_skip_node {
    struct skip_node node;
    struct y_key key;
};

int rand_lvl(void);

struct y_skip_node* y_skip_find(struct skip_list* l, struct y_key* key);
int y_skip_insert(struct skip_list* l, struct y_skip_node* elem);
int skip_erase(struct skip_list*l, struct y_key* key);

#endif