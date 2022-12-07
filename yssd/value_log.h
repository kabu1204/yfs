#ifndef YSSD_VALUE_LOG_H
#define YSSD_VALUE_LOG_H
#include "lsmtree.h"
#include "types.h"
#include "linux/hashtable.h"
#include "linux/kthread.h"
#include "linux/sched.h"
#include "linux/wait.h"
#include "linux/completion.h"
#include "linux/interrupt.h"

#define VLOG_HASH_TABLE_BITS   13
#define VLOG_N_FLUSH_PER_GC    5
#define VLOG_RESET_IN_MEM_SIZE 4
#define VLOG_GC_PAGES          (Y_VLOG_FLUSH_SIZE<<2) >> Y_PAGE_SHIFT

struct vlog_node {
    struct y_k2v* k2v;
    struct y_value* v;
    /*
        used to temporarily record the 
        offset to the flush start page;
    */
    unsigned int offset;
};

struct vlog_list_node {
    struct vlog_node vnode;

    struct list_head lhead;
};

struct vlog_hlist_node {
    struct vlog_node vnode;

    struct hlist_node hnode;
};

struct value_log {
    unsigned int tail1, tail2;
    unsigned int head;
    unsigned long in_mem_size;

    unsigned int n_flush;

    struct task_struct* write_thread;
    wait_queue_head_t waitq;
    rwlock_t rwlock, act_lk, inact_lk;

    struct hash_table {
        DECLARE_HASHTABLE(ht, VLOG_HASH_TABLE_BITS);
    } *active, *inactive;


    struct lsm_tree* lt;

    struct kmem_cache* vlist_slab;
    struct kmem_cache* k2v_slab;
}; 

struct value_log* vlog_create(void);

int vlog_append(struct value_log* vlog, struct y_k2v* k2v, struct y_value* val);

void vlog_flush(struct value_log* vlog);

void vlog_gc(struct value_log* vlog);

int vlog_get(struct value_log* vlog, struct y_k2v* k2v, struct y_value* v);

unsigned long vlog_node_dump_size(struct y_k2v* k2v, struct y_value* val);

unsigned long vlog_node_dump(struct vlog_node* vnode, char *buf);

unsigned long vlog_node_load(char *buf, struct vlog_node* vnode);

unsigned long vlog_read_value(char *buf, struct y_key* key, struct y_value* v);

void vlog_wakeup_or_block(struct value_log* vlog);

int write_deamon(void* arg);

#endif