#include "heap.h"
#include <linux/string.h>
#include <linux/printk.h>

inline void min_heap_init(struct min_heap* h, unsigned int cap, unsigned int size, min_heap_less_t less, min_heap_swap_t swap){
    h->size = size;
    h->cap = cap;
    h->len = 0;
    h->less = less;
    h->swap = swap;
}

inline void heap_down(struct min_heap *h, unsigned int pos){
    void *lc, *rc, *parent, *t, *arr;
    arr = h->arr;
    while(((pos<<1)+1)<h->len){
        lc = arr + ((pos<<1) + 1)*h->size;
        t = parent = arr + pos*h->size;
        if(h->less(lc, t)){
            t = lc;
        }
        if(((pos<<1)+2)<h->len){
            rc = arr + ((pos<<1)+2)*h->size;
            if(h->less(rc, t)){
                t = rc;
            }
        }
        if(t==parent) break;
        h->swap(t, parent);
        if(t==lc) pos = (pos<<1)+1;
        else pos = (pos<<1)+2;
    }
}

inline int min_heap_push(struct min_heap *h, void* p){
    unsigned int pos;
    void *arr, *parent, *child;

    if(h->len==h->cap){
        pr_warn("[heap] heap is full\n");
        return -1;
    }

    pos = h->len;
    arr = h->arr;

    memcpy(arr+pos*h->size, p, h->size);
    child = arr + pos*h->size;
    while(pos){
        parent = arr + ((pos-1)>>1)*h->size;
        if(h->less(parent, child)) break;
        h->swap(parent, child);
        child = parent;
        pos = (pos-1)>>1;
    }
    ++h->len;
    return 1;
}

inline int min_heap_pop(struct min_heap *h){
    if(unlikely(h->len==0)){
        pr_err("[heap] empty heap\n");
        return -1;
    }

    --h->len;
    if(h->len==0) return 1;
    memcpy(h->arr, h->arr + h->len*h->size, h->size);
    heap_down(h, 0);
    return 1;
}

inline int min_heap_replace_min(struct min_heap *h, void *p){
    memcpy(h->arr, p, h->size);
    heap_down(h, 0);
    return 1;
}

inline void min_heapify(struct min_heap *h){
    int i;
    if(unlikely(h->len>h->cap)){
        pr_err("[heap] len(%u) > cap(%u)\n", h->len, h->cap);
        return;
    }

    for(i=h->len>>1; i>=0; --i){
        heap_down(h, i);
    }
}

inline void* min_heap_min(struct min_heap* h){
    return h->arr;
}

inline int min_heap_empty(struct min_heap *h){
    return h->len==0;
}

inline void min_heap_clear(struct min_heap *h){
    h->len = 0;
}