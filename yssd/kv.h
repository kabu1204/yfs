#ifndef YSSD_KV_H
#define YSSD_KV_H

#include "types.h"
#include "rbkv.h"
#include "value_log.h"
#include "lsmtree.h"

struct lsm_tree lt;
struct value_log vlog;
struct mutex glk;

struct y_val_ptr unflush_ptr = {
    .page_no = OBJECT_VAL_UNFLUSH
};

void kv_init(void);

void kv_get(struct y_key* key, struct y_value* val);

void kv_set(struct y_key* key, struct y_value* val);

#endif
