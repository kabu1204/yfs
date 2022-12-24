#ifndef YSSD_MERGE_H
#define YSSD_MERGE_H
#include "types.h"

struct y_k2v_list_node {
    struct y_k2v k2v;
    struct y_k2v_list_node* next;
};

void merge_init(void);

int y_k2v_less(const void* left, const void* right);

void y_k2v_swap(void* left, void* right);


#endif