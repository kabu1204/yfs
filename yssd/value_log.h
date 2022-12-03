#ifndef YSSD_VALUE_LOG_H
#define YSSD_VALUE_LOG_H
#include "types.h"
#include "linux/hashtable.h"

#define VLOG_HASH_TABLE_BITS 13

struct vlog_node {
    struct y_k2v* k2v;
    struct y_value* v;

    struct hlist_node node;
};

struct value_log {
    unsigned int last_page_no;
    unsigned long in_mem_size;

    struct hlist_head ht[1<<VLOG_HASH_TABLE_BITS];
};

struct value_log* vlog_create(void);

int vlog_append(struct value_log* vlog, struct y_k2v* k2v, struct y_value* val);

void vlog_flush(struct value_log* vlog);

void vlog_gc(struct value_log* vlog);

struct y_value* vlog_get(struct y_k2v* k2v);

unsigned long vlog_dump_size(struct vlog_node* vnode);

#endif