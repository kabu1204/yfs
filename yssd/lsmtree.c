#include "rbkv.h"
#include "types.h"
#include "lsmtree.h"
#include "linux/slab.h"
#include "value_log.h"

struct lsm_tree* lsm_tree_create(void){
    struct lsm_tree* lt = kmalloc(sizeof(struct lsm_tree), GFP_KERNEL);
    lt->mem_table = RB_ROOT;
    return lt;
}

struct y_value* lsm_tree_get(struct lsm_tree* lt, struct y_key* key){
    // TODO: re-consider thread safety
    struct y_rb_node* node;
    node = y_rb_find(&lt->mem_table, key);
    if(likely(node)){
        if(node->kv.ptr.page_no==OBJECT_DEL) return NULL;
        return vlog_get(&node->kv);
    }
    // TODO: search in disk
    return NULL;
}

void lsm_tree_del(struct lsm_tree* lt, struct y_key* key){

}

void lsm_tree_iter(struct lsm_tree* lt, struct y_key* key){

}