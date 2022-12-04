#ifndef YSSD_VALUE_LOG_H
#define YSSD_VALUE_LOG_H
#include "types.h"
#include "linux/hashtable.h"

#define VLOG_HASH_TABLE_BITS 13

struct vlog_node {
    struct y_k2v* k2v;
    struct y_value* v;

    /*
        used to temporarily record the 
        offset to the flush start page;
    */
    unsigned int offset;

    struct hlist_node node;
};

struct value_log {
    unsigned int tail;
    unsigned int head;
    unsigned long in_mem_size;


    struct hlist_head ht[1<<VLOG_HASH_TABLE_BITS];
}; 

struct value_log* vlog_create(void);

int vlog_append(struct value_log* vlog, struct y_k2v* k2v, struct y_value* val);

void vlog_flush(struct value_log* vlog);

void vlog_gc(struct value_log* vlog);

struct y_value* vlog_get(struct value_log* vlog, struct y_k2v* k2v);

unsigned long vlog_dump_size(struct y_k2v* k2v, struct y_value* val);

unsigned long vlog_node_dump(struct vlog_node* vnode, char *buf);

#endif