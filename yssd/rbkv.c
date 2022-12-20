#include "rbkv.h"
#include "linux/slab.h"
#include "linux/random.h"

struct y_rb_node* y_rb_find(struct rb_root* root, struct y_key* key)
{
    int res;
    struct y_rb_node *cur;
    struct rb_node* node = root->rb_node;
    while(node)
    {
        cur = container_of(node, struct y_rb_node, node);
        res = y_key_cmp(key, &cur->kv.key);
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
        res = y_key_cmp(&elem->kv.key, &this->kv.key);
        parent = *new;
        if(res < 0) new = &((*new)->rb_left);
        else if(res > 0) new = &((*new)->rb_right);
        else {
            if(unlikely(elem->kv.timestamp < this->kv.timestamp)){
                return -1;
            }
            this->kv.ptr = elem->kv.ptr;
            this->kv.timestamp = elem->kv.timestamp;
            return 0;
        }
    }
    rb_link_node(&elem->node, parent, new);
    rb_insert_color(&elem->node, root);
    return 1;
}

void test_y_rbkv_insert(void){
    struct rb_root rt = RB_ROOT;
    struct y_rb_node* node[1000];
    struct rb_node* x;
    char buf[300];
    int i=0;
    for(i=0;i<1000;++i){
        node[i] = kmalloc(sizeof(struct y_rb_node), GFP_KERNEL);
        node[i]->kv.key.typ = 'm';
        strcpy(node[i]->kv.key.name, "hello");
        get_random_bytes(&node[i]->kv.key.ino, 4);
        sprint_y_key(buf, &node[i]->kv.key);
        pr_info("%d: %s\n", i, buf);
        if(!y_rb_insert(&rt, node[i])){
            pr_info("already exist\n");
        }
    }
    pr_info("ok1\n");
    for(i=0;i<1000;++i){
        if(!y_rb_find(&rt, &node[i]->kv.key)){
            pr_info("not find\n");
        }
    }
    pr_info("ok2\n");
    
    i=0;
    for(x=rb_first(&rt); x; x=rb_next(x)){
        struct y_key* k = &rb_entry(x, struct y_rb_node, node)->kv.key;
        sprint_y_key(buf, k);
        pr_info("%d: %s\n", i++, buf);
    }
    pr_info("ok3\n");
    for(i=0;i<1000;++i){
        rb_erase(&node[i]->node, &rt);
        kfree(node[i]);
    }
    pr_info("ok4\n");
}

void test_y_rbkv_update(void){
    struct rb_root rt = RB_ROOT;
    struct y_rb_node* node[1000];
    struct rb_node* x;
    char buf[300];
    int i=0;
    for(i=0;i<1000;++i){
        node[i] = kmalloc(sizeof(struct y_rb_node), GFP_KERNEL);
        node[i]->kv.key.typ = 'm';
        strcpy(node[i]->kv.key.name, "hello");
        node[i]->kv.key.ino = i%10;
        node[i]->kv.ptr.page_no = i;
        if(y_rb_insert(&rt, node[i]) && i>9){
            pr_info("should update: %d\n", i);
        }
    }
    i=0;
    for(x=rb_first(&rt); x; x=rb_next(x)){
        struct y_key* k = &rb_entry(x, struct y_rb_node, node)->kv.key;
        sprint_y_key(buf, k);
        pr_info("%d: (%s, %u)\n", i++, buf, rb_entry(x, struct y_rb_node, node)->kv.ptr.page_no);
    }
    for(i=0;i<1000;++i){
        rb_erase(&node[i]->node, &rt);
        kfree(node[i]);
    }
}