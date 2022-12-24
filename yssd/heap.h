#ifndef YSSD_HEAP_H
#define YSSD_HEAP_H

typedef int (*min_heap_less_t)(const void* left, const void* right);
typedef void (*min_heap_swap_t)(void* left, void* right);

struct min_heap {
    void* arr;
    min_heap_less_t less;
    min_heap_swap_t swap;
    void (*free)(void *p);
    unsigned int size;  // size of elem
    unsigned int len;  // number of elem in heap
    unsigned int cap;  // max capacity
};

void min_heap_init(struct min_heap* h, unsigned int cap, unsigned int size, min_heap_less_t less, min_heap_swap_t swap);
int min_heap_empty(struct min_heap *h);
int min_heap_push(struct min_heap *h, void *p);
int min_heap_pop(struct min_heap *h);
int min_heap_replace_min(struct min_heap *h, void *p);
void* min_heap_min(struct min_heap* h);
void min_heapify(struct min_heap *h);
void min_heap_clear(struct min_heap *h);

#endif