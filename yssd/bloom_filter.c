#include "bloom_filter.h"
#include "linux/slab.h"
#include "types.h"

struct bloom_filter* bloom_alloc(void){
    struct bloom_filter* bf = kzalloc(sizeof(struct bloom_filter), GFP_KERNEL);
    return bf;
}

void bloom_add(struct bloom_filter* bf, struct y_key* key){
    unsigned long hash = y_key_hash(key);
    unsigned long delta;
    int i;
    unsigned long bit;
    delta = (hash >> 33) | (hash << 31);
    for(i=0;i<Y_BLOOM_K;++i)
    {
        bit = hash % Y_BLOOM_M;
        bf->array[bit>>3] |= (0b1<<(bit & (0b111)));
        hash += delta;
    }
}

int bloom_contains(struct bloom_filter* bf, struct y_key* key){
    unsigned long hash = y_key_hash(key);
    unsigned long delta;
    int i;
    unsigned long bit;
    delta = (hash >> 33) | (hash << 31);
    for(i=0;i<Y_BLOOM_K;++i)
    {
        bit = hash % Y_BLOOM_M;
        if((bf->array[bit>>3] & (0b1<<(bit & (0b111))))==0) return 0;
        hash += delta;
    }
    return 1;
}