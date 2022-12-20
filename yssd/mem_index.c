#include "mem_index.h"
#include "types.h"

inline int y_rb_index_cmp(struct y_rb_index* left, struct y_rb_index* right){
    int res = y_key_cmp(&left->start, &right->start);
    if(res==0){
        if(left->blk.table_no!=right->blk.table_no) return (left->blk.table_no<right->blk.table_no)?-1:1;
        if(left->blk.block_no!=right->blk.block_no) return (left->blk.block_no<right->blk.block_no)?-1:1;
        return 0;
    }
    return res;
}

struct y_rb_index* y_rbi_find(struct rb_root* root, struct y_key* key){
    int res;
    struct y_rb_index *cur;
    struct rb_node* node = root->rb_node;
    while(node)
    {
        cur = container_of(node, struct y_rb_index, node);
        res = y_key_cmp(key, &cur->start);
        if(res>0) node = node->rb_right;
        else if(res<0) node = node->rb_left;
        else return cur;
    }
    return NULL;
}

int y_rbi_insert(struct rb_root* root, struct y_rb_index* elem){
    int res;
    struct y_rb_index *this;
    struct rb_node **new = &(root->rb_node), *parent = NULL;
    while (*new) {
        this = container_of(*new, struct y_rb_index, node);
        res = y_key_cmp(&elem->start, &this->start);
        if(unlikely(res==0)){
            if(elem->blk.table_no!=this->blk.table_no) res = (elem->blk.table_no>this->blk.table_no)?-1:1;
            else if(elem->blk.block_no!=this->blk.block_no) res = (elem->blk.block_no>this->blk.block_no)?-1:1;
            else res = 0;
        }
        parent = *new;
        if(res < 0) new = &((*new)->rb_left);
        else if(res > 0) new = &((*new)->rb_right);
        else {
            pr_warn("same rb_index\n");
            return 0;
        }
	}
	rb_link_node(&elem->node, parent, new);
	rb_insert_color(&elem->node, root);
	return 1;
}

/*
    find first rb_index whose start >= key
*/
struct y_rb_index* y_rbi_lower_bound(struct rb_root* root, struct y_key* key){
    int res;
    struct y_rb_index *cur;
    struct rb_node* node = root->rb_node, *ret=rb_first(root);
    while(node)
    {
        cur = container_of(node, struct y_rb_index, node);
        res = y_key_cmp(key, &cur->start);
        if(res>0){  // cur < key
            ret = rb_next(node);
            node = node->rb_right;
        }
        else node = node->rb_left;    // cur >= key
    }
    return (ret!=NULL)?container_of(ret, struct y_rb_index, node):NULL;
}

/*
    find first rb_index whose start > key
*/
struct y_rb_index* y_rbi_upper_bound(struct rb_root* root, struct y_key* key){
    int res;
    struct y_rb_index *cur;
    struct rb_node* node = root->rb_node, *ret=rb_first(root);
    while(node)
    {
        cur = container_of(node, struct y_rb_index, node);
        res = y_key_cmp(key, &cur->start);
        if(res>=0){  // cur <= key
            ret = rb_next(node);
            node = node->rb_right;
        }
        else node = node->rb_left;    // cur > key
    }
    return (ret!=NULL)?container_of(ret, struct y_rb_index, node):NULL;
}

