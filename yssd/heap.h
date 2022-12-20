#ifndef YSSD_HEAP_H
#define YSSD_HEAP_H

struct min_heap {
    void* arr;
    int (*less)(const void* left, const void* right);
    void (*swap)(void *left, void *right);
    void (*free)(void *p);
    unsigned int size;  // size of elem
    unsigned int len;  // number of elem in heap
    unsigned int cap;  // max capacity
};

int min_heap_push(struct min_heap *h, void *p);
int min_heap_pop(struct min_heap *h);
int min_heap_replace_min(struct min_heap *h, void *p);
void* min_heap_min(struct min_heap* h);
void min_heapify(struct min_heap *h);

#endif