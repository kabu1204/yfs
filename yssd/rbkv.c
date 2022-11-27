#include "rbkv.h"


/* 
  -1: left < right 
   0: left == right
   1: left > right
*/
int y_rb_cmp(struct y_key *left, struct y_key *right)
{
    if(left->typ!=right->typ) return (left->typ<right->typ)?-1:1;
    if(left->ino!=right->ino) return (left->ino<right->ino)?-1:1;
    return strcmp(left->name, right->name);
}

struct y_rb_node* y_rb_find(struct rb_root* root, struct y_key* key)
{
    int res;
    struct y_rb_node *cur;
    struct rb_node* node = root->rb_node;
    while(node)
    {
        cur = container_of(node, struct y_rb_node, node);
        res = y_rb_cmp(key, &cur->key);
        if(res>0) node = node->rb_right;
        else if(res<0) node = node->rb_left;
        else return cur;
    }
    return NULL;
}

int y_rb_insert(struct rb_root* root, struct y_rb_node* elem)
{
    int res;
    struct y_rb_node *this;
    struct rb_node **new = &(root->rb_node), *parent = NULL;
	while (*new) {
		this = container_of(*new, struct y_rb_node, node);
		res = y_rb_cmp(&elem->key, &this->key);
		parent = *new;
		if(res < 0) new = &((*new)->rb_left);
		else if(res > 0) new = &((*new)->rb_right);
		else return 0;
	}
	rb_link_node(&elem->node, parent, new);
	rb_insert_color(&elem->node, root);
	return 1;
}

