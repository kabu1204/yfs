#include "lsmtree.h"
#include "types.h"
#include <linux/slab.h>
#include <linux/rbtree.h>

void memtable_flush(struct lsm_tree* lt){
    
}

void compact(struct lsm_tree* lt){

}

void wakeup_compact(struct lsm_tree* lt){
    pr_info("[lsmtree] flush triggered\n");
    wait_event_interruptible(lt->waitq, lt->imm_table==NULL);
    lt->imm_table = lt->mem_table;
    lt->mem_table = kzalloc(sizeof(struct rb_root), GFP_KERNEL);
    lt->imm_size = lt->mem_size;
    lt->mem_size = LSM_TREE_RESET_IN_MEM_SIZE;
    wake_up_interruptible(&lt->waitq);
}

inline unsigned int lsm_k2v_size(struct y_key* key)
{
    return 21+((key->typ==Y_KV_META)?(1+key->len):4);
}

int compact_deamon(void* arg){
    struct lsm_tree* lt = arg;
    while(1){
        wait_event_interruptible(lt->waitq, lt->imm_table!=NULL);
        pr_info("compaction thread wake up\n");
        memtable_flush(lt);
        pr_info("[lsmtree] flush finished\n");
        ++lt->n_flush;
        if(lt->n_flush % LSM_TREE_FLUSH_PER_COMPACT == 0){
            pr_info("[compact] major compaction triggered\n");
            compact(lt);
            pr_info("[compact] major compaction finished\n");
        }
        wake_up_interruptible(&lt->waitq);
    }
    return 0;
}