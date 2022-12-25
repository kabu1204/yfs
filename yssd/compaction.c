#include "bloom_filter.h"
#include "lsmtree.h"
#include "phys_io.h"
#include "rbkv.h"
#include "types.h"
#include <linux/slab.h>
#include "mem_index.h"
#include <linux/rbtree.h>
#include <linux/kthread.h>

void memtable_flush(struct lsm_tree* lt){
    char* buf;
    unsigned int i_block;
    unsigned int k2v_size;
    unsigned int prev_size;
    int start, i;
    unsigned long basep;
    unsigned long off;
    unsigned long metap;
    struct bloom_filter* bfs[Y_DATA_BLOCK_PER_TABLE];
    struct y_rb_node* rbnode, *tmp;
    struct y_rb_index* indices[Y_DATA_BLOCK_PER_TABLE];
    struct rb_node* cur, *prev;
    struct y_k2v* k2v;

    prev_size = k2v_size = 0;
    metap = Y_META_BLOCK_HEADER_SIZE;
    i_block = -1;
    basep = 0;
    off = 0;
    start = 0;
    buf = lt->comp_buf;

    read_lock(&lt->imm_lk);
    if(!rb_first(lt->imm_table)){
        read_unlock(&lt->imm_lk);
        return;
    }

    off = Y_BLOCK_SIZE;
    for(prev=NULL, cur = rb_first(lt->imm_table); cur; prev=cur, cur = rb_next(cur)){
        rbnode = rb_entry(cur, struct y_rb_node, node);
        k2v = &rbnode->kv;
        if(likely(prev))
            rb_entry(prev, struct y_rb_node, node)->nxt = rbnode;
        k2v_size = lsm_k2v_size(&k2v->key);
        if(unlikely(off + k2v_size > Y_BLOCK_SIZE)){
            if(likely(prev)){
                memcpy(buf+metap, buf+basep+off-prev_size, prev_size);  // block end_key
                metap += Y_MAX_K2V_SIZE;
                indices[i_block]->end = rb_entry(prev, struct y_rb_node, node)->kv.key;
                // pr_info("[compact] (%u, %u) end=  %u\n", lt->nr_l0, i_block, indices[i_block]->end.ino);
            }
            if(off<Y_BLOCK_SIZE){
                memset(buf+basep+off, 0, Y_BLOCK_SIZE-off);
            }
            bfs[++i_block] = bloom_alloc();
            indices[i_block] = kzalloc(sizeof(struct y_rb_index), GFP_KERNEL);
            indices[i_block]->start = k2v->key;
            basep += Y_BLOCK_SIZE;
            start = 1;
            off = 0;
        }
        bloom_add(bfs[i_block], &k2v->key);
        if(unlikely(k2v_size != dump_k2v(buf+basep+off, &k2v->key, k2v->ptr, k2v->timestamp))) {
            pr_err("[compact] k2v_size unmatch\n");
        }
        if(unlikely(start)){
            memcpy(buf+metap, buf+basep+off, k2v_size); // block start_key
            // pr_info("[compact] (%u, %u) start=%u\n", lt->nr_l0, i_block, k2v->key.ino);
            metap += Y_MAX_K2V_SIZE;
            start = 0;
        }
        off += k2v_size;
        prev_size = k2v_size;
    }
    rb_entry(prev, struct y_rb_node, node)->nxt = NULL;
    read_unlock(&lt->imm_lk);

    memcpy(buf, buf+Y_BLOCK_SIZE, lsm_k2v_size(&rb_entry(rb_first(lt->imm_table), struct y_rb_node, node)->kv.key));   // table start_key

    memcpy(buf+Y_MAX_K2V_SIZE, buf+basep+off-prev_size, prev_size);
    memcpy(buf+metap, buf+Y_MAX_K2V_SIZE, prev_size);  // table end_key
    indices[i_block]->end = rb_entry(prev, struct y_rb_node, node)->kv.key;
    metap += Y_MAX_K2V_SIZE;

    *(unsigned int*)(buf+Y_MAX_K2V_SIZE*2) = i_block+1;

    for(i=0;i<=i_block;++i){
        memcpy(buf+metap, bfs[i], sizeof(struct bloom_filter));
        kfree(bfs[i]);
        metap += sizeof(struct bloom_filter);
        indices[i]->blk.table_no = lt->nr_l0;
        indices[i]->blk.block_no = i;
    }

    pr_info("[compact] dump bloom filters finished, metap = %luKB\n", metap>>10);

    yssd_write_phys_pages(buf, lt->p0, Y_TABLE_SIZE>>Y_PAGE_SHIFT);

    pr_info("[compact] p0 moved from %u to %lu\n", lt->p0, lt->p0+(Y_TABLE_SIZE>>Y_PAGE_SHIFT));
    lt->p0 += Y_TABLE_SIZE>>Y_PAGE_SHIFT;

    pr_info("[compact] start: %u end: %u\n", rb_entry(rb_first(lt->imm_table), struct y_rb_node, node)->kv.key.ino, rb_entry(rb_last(lt->imm_table), struct y_rb_node, node)->kv.key.ino);

    write_lock(&lt->index_lk);
    for(i=0;i<=i_block;++i){
        y_rbi_insert(lt->mem_index, indices[i]);
    }
    lt->mem_index_nr += i_block+1;
    ++lt->nr_l0;
    // for(rbn=rb_first(lt->mem_index), i=0; rbn; ++i, rbn = rb_next(rbn)){
    //     rbi = rb_entry(rbn, struct y_rb_index, node);
    //     pr_info("[compact] rbi %d: (%u, %u)\n", i, rbi->start.ino, rbi->end.ino);
    // }
    write_unlock(&lt->index_lk);

    write_lock(&lt->imm_lk);
    for(rbnode=rb_entry(rb_first(lt->imm_table), struct y_rb_node, node), tmp=rbnode->nxt; tmp; rbnode=tmp, tmp=rbnode->nxt){
        kmem_cache_free(lt->rb_node_slab, rbnode);
    }
    kmem_cache_free(lt->rb_node_slab, rbnode);
    kfree(lt->imm_table);
    lt->imm_table = NULL;
    write_unlock(&lt->imm_lk);
}



void compact(struct lsm_tree* lt){

}

/*
    Can be called from either access thread nor vlog write thread.
    The caller must be holding a write lock(lt->lk).
*/
void wakeup_compact(struct lsm_tree* lt){
    pr_info("[lsmtree] flush triggered\n");
    wait_event_interruptible(lt->waitq, lt->imm_table==NULL);
    lt->imm_table = lt->mem_table;
    lt->mem_table = kzalloc(sizeof(struct rb_root), GFP_KERNEL);
    lt->imm_size = lt->mem_size;
    lt->mem_size = LSM_TREE_RESET_IN_MEM_SIZE;
    lt->max_k2v_size = 0;
    wake_up_interruptible(&lt->waitq);
}

int compact_deamon(void* arg){
    struct lsm_tree* lt = arg;
    while(!lt->compact_thread_stop){
        wait_event_interruptible(lt->waitq, lt->imm_table!=NULL);
        if(lt->compact_thread_stop){
            break;
        }
        pr_info("[lsmtree] compaction thread wake up\n");
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
    pr_info("[compact] compact thread stopped\n");
    return 0;
}

inline unsigned int lsm_k2v_size(struct y_key* key)
{
    return 22+((key->typ==Y_KV_META)?(1+key->len):4);
}

inline int k2v_valid(const char *buf){
    return buf[0]=='#';
}

inline unsigned int dump_k2v(char* buf, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp){
    unsigned int p=1;
    buf[0] = '#';
    buf[p++] = key->typ;
    *(unsigned int*)(buf + p) = key->ino;
    p += 4;
    if(key->typ==Y_KV_META){
        *(char*)(buf + p) = (char)(key->len);
        ++p;
        memcpy(buf + p, key->name, key->len);
        p += key->len;
    } else {
        *(unsigned int*)(buf + p) = key->len;
        p += 4;
    }
    *(unsigned long*)(buf + p) = timestamp;
    p += 8;
    *(struct y_val_ptr*)(buf + p) = ptr;
    p += sizeof(struct y_val_ptr);
    return p;
}

inline unsigned int read_k2v(char *buf, struct y_k2v* k2v){
    unsigned int p=1;
    buf[0] = '#';
    if(unlikely(buf[0]!='#')){
        pr_err("[read_k2v] format err\n");
        return 0;
    }
    k2v->key.typ = buf[p++];
    k2v->key.ino = *(unsigned int*)(buf + p);
    p += 4;
    if(k2v->key.typ==Y_KV_META){
        k2v->key.len = *(char*)(buf + p);
        ++p;
        memcpy(k2v->key.name, buf + p, k2v->key.len);
        p += k2v->key.len;
    } else {
        k2v->key.len = *(unsigned int*)(buf + p);
        p += 4;
    }
    k2v->timestamp = *(unsigned long*)(buf + p);
    p += 8;
    k2v->ptr = *(struct y_val_ptr*)(buf + p);
    p += sizeof(struct y_val_ptr);
    return p;
}

