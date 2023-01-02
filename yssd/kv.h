#ifndef YSSD_KV_H
#define YSSD_KV_H

#include "types.h"
#include "rbkv.h"
#include "value_log.h"
#include "lsmtree.h"

void kv_init(void);

int kv_get(struct y_key* key, struct y_value* val);

void kv_set(struct y_key* key, struct y_value* val);

void kv_del(struct y_key* key);

int kv_iter(char typ, unsigned int ino, struct y_key* key, struct y_value* val);

int kv_next(struct y_key* key, struct y_value* val);

void mannual_gc(void);

void kv_close(void);

#endif
