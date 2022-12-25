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
    struct y_key key;
    struct y_value v;
    unsigned long timestamp;
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
    unsigned long inact_size;

    unsigned int n_flush;
    unsigned int catchup;

    int write_thread_stop;

    struct task_struct* write_thread;
    wait_queue_head_t waitq;
    rwlock_t rwlock, act_lk, inact_lk;

    struct hash_table {
        DECLARE_HASHTABLE(ht, VLOG_HASH_TABLE_BITS);
    } *active, *inactive;


    struct lsm_tree* lt;

    struct kmem_cache* vl_slab;
    struct kmem_cache* vh_slab;
};

enum gc_stat_item {
    NR_ROUND,
    NR_PAGES_COLLECTED,
    NR_PAGES_FREED,
    NR_VALID_KV,
    TIME_PER_GC_NS,

    N_TOTAL_ITEMS
};

void vlog_init(struct value_log* vlog);

int vlog_append(struct value_log* vlog, struct y_key* key, struct y_value* val, unsigned long timestamp);

void vlog_flush(struct value_log* vlog);

void vlog_gc(struct value_log* vlog);

int vlog_get(struct value_log* vlog, struct y_key* key, struct y_val_ptr ptr, struct y_value* v);

void valcpy(struct y_value* to, const struct y_value* from);

unsigned long vlog_dump_size(struct y_key* key, struct y_value* val);

unsigned long vlog_node_dump(struct vlog_node* vnode, char *buf);

unsigned long vlog_node_load(char *buf, struct vlog_node* vnode);

unsigned long vlog_read_value(char *buf, struct y_key* key, struct y_value* v);

void vlog_flush_sync(struct value_log* vlog);

void vlog_wakeup_or_block(struct value_log* vlog);

int write_deamon(void* arg);

void init_gc_stat(void);

void gc_early(struct value_log* vlog);

void vlog_close(struct value_log* vlog);

#endif