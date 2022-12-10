#ifndef YSSD_KV_H
#define YSSD_KV_H

#include "types.h"
#include "rbkv.h"
#include "value_log.h"
#include "lsmtree.h"

void kv_init(void);

void kv_get(struct y_key* key, struct y_value* val);

void kv_set(struct y_key* key, struct y_value* val);

#endif
