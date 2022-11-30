#include "bloom_filter.h"
#include "linux/slab.h"

unsigned long sdbm_hash(const unsigned char *str)
{
    unsigned long hash = 0;
    int c;
    while (c = *str++)
        hash = c + (hash << 6) + (hash << 16) - hash;
    return hash;
}

struct bloom_filter* bloom_alloc(void){
    struct bloom_filter* bf = kmalloc(sizeof(struct bloom_filter), GFP_KERNEL);
    memset(bf, 0, sizeof(struct bloom_filter));
    return bf;
}

void bloom_add(struct bloom_filter* bf, const char* key){
    unsigned long hash = sdbm_hash(key);
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

int bloom_contains(struct bloom_filter* bf, const char* key){
    unsigned long hash = sdbm_hash(key);
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