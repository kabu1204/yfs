#include "types.h"
#include <linux/string.h>

/* 
  -1: left < right 
  0: left == right
  1: left > right
*/
int y_key_cmp(struct y_key *left, struct y_key *right)
{
    if(left->typ!=right->typ) return (left->typ<right->typ)?-1:1;
    if(left->ino!=right->ino) return (left->ino<right->ino)?-1:1;
    return strcmp(left->name, right->name);
}

unsigned int key_dump_size(struct y_k2v* k2v)
{
    unsigned int res=17; // typ(1) + ino(4) + val_ptr(12)
    if(k2v->key.typ==Y_KV_META) res += 1 + k2v->key.len;
    else res += 4;
    return res;
}