#include "bloom_filter.h"
#include "heap.h"
#include "mem_index.h"
#include "phys_io.h"
#include "rbkv.h"
#include "types.h"
#include "lsmtree.h"
#include "linux/slab.h"
#include "value_log.h"
#include <linux/mm.h>
#include <linux/wait.h>

unsigned int counter=0;

struct y_val_ptr not_found_ptr = {
    .page_no = OBJECT_NOT_FOUND,
};

void lsm_tree_init(struct lsm_tree* lt){
    lt->mem_table = kzalloc(sizeof(struct rb_root), GFP_KERNEL);
    lt->mem_index = kzalloc(sizeof(struct rb_root), GFP_KERNEL);
    lt->imm_table = NULL;
    lt->nr_l0 = 0;
    lt->nr_l1 = 0;
    lt->n_flush = 0;
    lt->max_k2v_size = 0;
    lt->mem_index_nr = 0;
    lt->compact_thread_stop = 0;
    lt->p0 = LSM_TREE_LEVEL0_START_PAGE;
    lt->p1 = LSM_TREE_LEVEL1_START_PAGE;
    lt->mem_size = LSM_TREE_RESET_IN_MEM_SIZE;
    lt->comp_buf = kvmalloc(Y_TABLE_SIZE, GFP_KERNEL);
    lt->read_buf = kvmalloc(Y_BLOCK_SIZE, GFP_KERNEL);
    min_heap_init(&lt->hp, 64, sizeof(struct y_block), y_block_greater, y_block_swap);
    lt->hp.arr = kvmalloc(lt->hp.cap*sizeof(struct y_block), GFP_KERNEL);
    init_waitqueue_head(&lt->waitq);
    lt->compact_thread = kthread_create(compact_deamon, lt, "yssd compact thread");
    lt->rb_node_slab = kmem_cache_create("yssd_lsm_tree_rb_node", sizeof(struct y_rb_node), 0, SLAB_HWCACHE_ALIGN, NULL);
    rwlock_init(&lt->mem_lk);
    rwlock_init(&lt->imm_lk);
    rwlock_init(&lt->ext_lk);
    rwlock_init(&lt->index_lk);
    mutex_init(&lt->hp_lk);

    pr_info("[lsmtree] Y_BLOCK_SIZE: %luB(%luKB)\n", Y_BLOCK_SIZE, Y_BLOCK_SIZE>>10);
    pr_info("[lsmtree] Y_TABLE_SIZE: %luB(%luKB)\n", Y_TABLE_SIZE, Y_TABLE_SIZE>>10);
    pr_info("[lsmtree] Y_BLOOM_SIZE_BYTES: %uB\n", Y_BLOOM_SIZE_BYTES);
    pr_info("[lsmtree] Y_DATA_BLOCK_PER_TABLE: %u\n", Y_DATA_BLOCK_PER_TABLE);
    pr_info("[lsmtree] Y_TABLE_DATA_SIZE: %uKB\n", Y_TABLE_DATA_SIZE>>10);
}

struct y_val_ptr lsm_tree_get(struct lsm_tree* lt, struct y_key* key){
    struct y_rb_node* node;
    struct y_k2v* k2v;
    struct y_val_ptr ptr;
    read_lock(&lt->ext_lk);

    read_lock(&lt->mem_lk);
    node = y_rb_find(lt->mem_table, key);
    if(likely(node)){
        read_unlock(&lt->mem_lk);
        read_unlock(&lt->ext_lk);
        return node->kv.ptr;
    }
    read_unlock(&lt->mem_lk);
    read_lock(&lt->imm_lk);
    if(!lt->imm_table){
        read_unlock(&lt->imm_lk);
        goto slow;
    }
    node = y_rb_find(lt->imm_table, key);
    if(likely(node)){
        read_unlock(&lt->imm_lk);
        read_unlock(&lt->ext_lk);
        return node->kv.ptr;
    }
    read_unlock(&lt->imm_lk);
    
slow:
    // pr_info("slow\n");
    k2v = lsm_tree_get_slow(lt, key);
    if(unlikely(!k2v)){
        read_unlock(&lt->ext_lk);
        return not_found_ptr;
    }
    ptr = k2v->ptr;
    kfree(k2v);

    read_unlock(&lt->ext_lk);
    return ptr;
}

struct y_k2v* lsm_tree_get_slow(struct lsm_tree* lt, struct y_key* key){
    struct y_k2v* k2v;
    struct y_rb_index *hi, *rbi;
    struct rb_node* cur;
    struct min_heap* h = &lt->hp;
    struct y_block blk;
    struct bloom_filter* bf;
    unsigned int p = 0, size;
    char* buf;

    mutex_lock(&lt->hp_lk);
    buf = lt->read_buf;
    k2v = kzalloc(sizeof(struct y_k2v), GFP_KERNEL);

    if(unlikely(lt->mem_index_nr>h->cap)){
        kvfree(h->arr);
        h->cap <<= 1;
        h->arr = kvmalloc(h->cap*h->size, GFP_KERNEL);
    }

    read_lock(&lt->index_lk);
    hi = y_rbi_upper_bound(lt->mem_index, key);
    cur = (hi) ? rb_prev(&hi->node) : rb_last(lt->mem_index);
    for(; cur; cur = rb_prev(cur)){
        rbi = rb_entry(cur, struct y_rb_index, node);
        if(y_key_cmp(&rbi->end, key)>=0){
            min_heap_push(h, &rbi->blk);
        }
    }

    while(!min_heap_empty(h)){
        blk = *(struct y_block*)min_heap_min(h);
        min_heap_pop(h);


        size = Y_META_BLOCK_BF_OFFSET+sizeof(struct bloom_filter)*blk.block_no;
        p = (size>>Y_PAGE_SHIFT)<<Y_PAGE_SHIFT;
        size = size & (Y_PAGE_SIZE-1);

        yssd_read_phys_page(buf, LSM_TREE_LEVEL0_START_PAGE+blk.table_no*(Y_TABLE_SIZE>>Y_PAGE_SHIFT)+(p>>Y_PAGE_SHIFT));
        
        // if(unlikely(read_k2v(buf+Y_META_BLOCK_HEADER_SIZE+Y_MAX_K2V_SIZE*2*blk.block_no, k2v)==0)){
        //     pr_warn("[slow get] broken start key of (%u, %u)\n", blk.table_no, blk.table_no);
        // }

        // if(unlikely(y_key_cmp(&k2v->key, key)>0)){
        //     pr_warn("[slow get] error start key, ino=%u, want=%u, tno=%u, bno=%u, page_no=%lu\n", k2v->key.ino, key->ino, blk.table_no, blk.block_no, LSM_TREE_LEVEL0_START_PAGE+blk.table_no*(Y_TABLE_SIZE>>Y_PAGE_SHIFT));
        //     cur = (hi) ? rb_prev(&hi->node) : rb_last(lt->mem_index);
        //     for(; cur; cur = rb_prev(cur)){
        //         rbi = rb_entry(cur, struct y_rb_index, node);
        //         if(y_key_cmp(&rbi->end, key)>=0){
        //             pr_info("[slow] (%u, %u), (%u, %u)\n", rbi->start.ino, rbi->end.ino, rbi->blk.table_no, rbi->blk.block_no);
        //         }
        //     }
        // }

        // if(unlikely(read_k2v(buf+Y_META_BLOCK_HEADER_SIZE+Y_MAX_K2V_SIZE*2*blk.block_no+Y_MAX_K2V_SIZE, k2v)==0)){
        //     pr_warn("[slow get] broken end key of (%u, %u)", blk.table_no, blk.table_no);
        // }

        // if(unlikely(y_key_cmp(&k2v->key, key)<0)){
        //     pr_warn("[slow get] error end key, ino=%u\n", k2v->key.ino);
        // }

        bf = (struct bloom_filter*)(buf+size);

        if(bloom_contains(bf, key)){
            yssd_read_phys_pages(buf, LSM_TREE_LEVEL0_START_PAGE+blk.table_no*(Y_TABLE_SIZE>>Y_PAGE_SHIFT)+(blk.block_no+1)*(Y_BLOCK_SIZE>>Y_PAGE_SHIFT), Y_BLOCK_SIZE>>Y_PAGE_SHIFT);

            p = 0;
            while(p<Y_BLOCK_SIZE && (size = read_k2v(buf+p, k2v))!=0){
                if(y_key_cmp(key, &k2v->key)==0){
                    goto out;
                }
                p += size;
            }
        }
    }
    kfree(k2v);
    k2v = NULL;
out:
    min_heap_clear(h);
    read_unlock(&lt->index_lk);
    mutex_unlock(&lt->hp_lk);

    return k2v;
}

inline int y_block_greater(const void* left, const void* right){
    struct y_block l = *(struct y_block*)left;
    struct y_block r = *(struct y_block*)right;
    char a = l.table_no>=LSM_TREE_FLUSH_PER_COMPACT;
    char b = r.table_no<LSM_TREE_FLUSH_PER_COMPACT;
    if(a && b) return 0;
    if(!a && !b) return 1;
    if(l.table_no!=r.table_no) return l.table_no>r.table_no;
    return l.block_no>r.block_no;
}

void y_block_swap(void* left, void* right){
    struct y_block t = *(struct y_block*)left;
    *(struct y_block*)left = *(struct y_block*)right;
    *(struct y_block*)right = t;
}

void lsm_tree_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp){
    struct y_rb_node* node;
    unsigned int k2v_size;
    int res;
    k2v_size = lsm_k2v_size(key);
    if(unlikely(k2v_size>Y_MAX_K2V_SIZE)){
        pr_err("[lsmtree] k2v exceeds size limit(%u)", Y_MAX_K2V_SIZE);
        return;
    }

    write_lock(&lt->ext_lk);
    // if(counter % 4096*3 == 0){
    //     pr_info("[lsmtree] mem_size: %uKB\n", lt->mem_size>>10);
    // }
    // ++counter;
    if(unlikely(exceed_table_size(lt->mem_size, k2v_size, lt->max_k2v_size))){
        wakeup_compact(lt);
    }
    
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr = ptr;
    node->kv.timestamp = timestamp;

    write_lock(&lt->mem_lk);
    res = y_rb_insert(lt->mem_table, node);
    if(unlikely(res==-1)){
        pr_warn("invalid timestamp\n");
        kmem_cache_free(lt->rb_node_slab, node);
    }
    if(res==0){ // update
        kmem_cache_free(lt->rb_node_slab, node);
    } else {    // newly insert
        lt->mem_size += k2v_size;
    }
    lt->max_k2v_size = max(k2v_size, lt->max_k2v_size);
    write_unlock(&lt->mem_lk);

    write_unlock(&lt->ext_lk);
}

int lsm_tree_get_and_set(struct lsm_tree* lt, struct y_key* key, struct y_val_ptr ptr, unsigned long timestamp){
    struct y_rb_node* node;
    struct y_k2v* k2v;
    unsigned int k2v_size;
    int res;
    k2v_size = lsm_k2v_size(key);
    if(unlikely(k2v_size>Y_MAX_K2V_SIZE)){
        pr_err("[lsmtree] k2v exceeds size limit(%u)", Y_MAX_K2V_SIZE);
        return -1;
    }

    write_lock(&lt->ext_lk);
    // if(counter % 4096*3 == 0){
    //     pr_info("[lsmtree] mem_size: %uKB\n", lt->mem_size>>10);
    // }
    // ++counter;
    if(unlikely(exceed_table_size(lt->mem_size, k2v_size, lt->max_k2v_size))){
        wakeup_compact(lt);
    }

    read_lock(&lt->mem_lk);
    node = y_rb_find(lt->mem_table, key);
    if(likely(node)){
        if(node->kv.timestamp > timestamp){
            // pr_info("[lsmtree] found newer record in memtable\n");
            read_unlock(&lt->mem_lk);
            write_unlock(&lt->ext_lk);
            return 0;
        }
        read_unlock(&lt->mem_lk);
        goto set;
    }
    read_unlock(&lt->mem_lk);

    read_lock(&lt->imm_lk);
    if(!lt->imm_table){
        read_unlock(&lt->imm_lk);
        goto slow;
    }
    node = y_rb_find(lt->imm_table, key);
    if(likely(node)){
        if(node->kv.timestamp > timestamp){
            pr_info("[lsmtree] found newer record in imm_table\n");
            read_unlock(&lt->imm_lk);
            write_unlock(&lt->ext_lk);
            return 0;
        }
        read_unlock(&lt->imm_lk);
        goto set;
    }
    read_unlock(&lt->imm_lk);

slow:
    // pr_info("[lsmtree] get_and_set search in disk\n");
    k2v = lsm_tree_get_slow(lt, key);
    if(unlikely(!k2v)){
        pr_warn("[lsmtree] get_and_set k2v deleted: %u\n", key->ino);
        write_unlock(&lt->ext_lk);
        return 0;
    }
    if(k2v->timestamp > timestamp){
        pr_info("[lsmtree] found newer record on disk\n");
        write_unlock(&lt->ext_lk);
        kfree(k2v);
        return 0;
    }
    kfree(k2v);

set:
    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr = ptr;
    node->kv.timestamp = timestamp;

    write_lock(&lt->mem_lk);
    res = y_rb_insert(lt->mem_table, node);
    if(res==-1){
        pr_warn("invalid timestamp\n");
        kmem_cache_free(lt->rb_node_slab, node);
    }
    if(res==0){ // update
        kmem_cache_free(lt->rb_node_slab, node);
    } else {    // newly insert
        lt->mem_size += k2v_size;
    }
    lt->max_k2v_size = max(k2v_size, lt->max_k2v_size);
    write_unlock(&lt->mem_lk);

    write_unlock(&lt->ext_lk);
    return 1;
}

void lsm_tree_del(struct lsm_tree* lt, struct y_key* key, unsigned long timestamp){
    struct y_rb_node* node;
    unsigned int k2v_size;
    int res;
    k2v_size = lsm_k2v_size(key);
    if(unlikely(k2v_size>Y_MAX_K2V_SIZE)){
        pr_err("[lsmtree] k2v exceeds size limit(%u)", Y_MAX_K2V_SIZE);
        return;
    }

    write_lock(&lt->ext_lk);
    if(unlikely(exceed_table_size(lt->mem_size, k2v_size, lt->max_k2v_size))){
        wakeup_compact(lt);
    }

    node = kmem_cache_alloc(lt->rb_node_slab, GFP_KERNEL);
    node->kv.key = *key;
    node->kv.ptr.page_no = OBJECT_DEL;
    node->kv.timestamp = timestamp;

    write_lock(&lt->mem_lk);
    res = y_rb_insert(lt->mem_table, node);
    if(res==-1){
        pr_warn("invalid timestamp\n");
        kmem_cache_free(lt->rb_node_slab, node);
    }
    if(res==0){ // update
        kmem_cache_free(lt->rb_node_slab, node);
    } else {    // newly insert
        lt->mem_size += k2v_size;
    }
    lt->max_k2v_size = max(k2v_size, lt->max_k2v_size);
    write_unlock(&lt->mem_lk);

    write_unlock(&lt->ext_lk);
}

struct y_val_ptr* lsm_tree_iter(struct lsm_tree* lt, struct y_key* key, unsigned int n){
    struct y_k2v* k2v;
    struct y_key k;
    struct y_val_ptr* ptr;
    int i;
    ptr = kvmalloc(n*sizeof(struct y_val_ptr), GFP_KERNEL);
    k = *key;
    for(i=0;i<n;++i){
        k2v = lsm_tree_get_upper_bound(lt, &k);
        ptr[i] = k2v->ptr;
        if(k2v->ptr.page_no==OBJECT_NOT_FOUND){
            kfree(k2v);
            break;
        }
        k = k2v->key;
        kfree(k2v);
    }
    return ptr;
}

struct y_k2v* lsm_tree_get_upper_bound(struct lsm_tree* lt, struct y_key* key){
    struct y_rb_node* node;
    struct y_k2v* k2v, *t;
    k2v = kmalloc(sizeof(struct y_k2v), GFP_KERNEL);
    k2v->ptr.page_no = OBJECT_NOT_FOUND;

    pr_info("[upper] %c:%u:%s\n", key->typ, key->ino, key->name);

    read_lock(&lt->ext_lk);

    // k2v = lsm_tree_get_slow_upper_bound(lt, key);

    read_lock(&lt->mem_lk);
    node = y_rb_upper_bound(lt->mem_table, key);
    if(likely(node)){
        *k2v = node->kv;
    }
    read_unlock(&lt->mem_lk);
    read_lock(&lt->imm_lk);
    if(!lt->imm_table){
        read_unlock(&lt->imm_lk);
        goto slow;
    }
    node = y_rb_upper_bound(lt->imm_table, key);
    if(likely(node)){
        if(y_key_cmp(&node->kv.key, &k2v->key)<0){
            *k2v = node->kv;
        }
    }
    read_unlock(&lt->imm_lk);
    
slow:
    t = lsm_tree_get_slow_upper_bound(lt, key);
    if(t->ptr.page_no!=OBJECT_NOT_FOUND && y_key_cmp(&t->key, &k2v->key)<0){
        kfree(k2v);
        k2v = t;
    }
    read_unlock(&lt->ext_lk);
    return k2v;
}

void lsm_tree_close(struct lsm_tree* lt){
    struct y_rb_index* rbi, *tmp;
    struct rb_node *cur, *prev;

    write_lock(&lt->ext_lk);
    wait_event_interruptible(lt->waitq, lt->imm_table==NULL);
    lt->imm_table = lt->mem_table;
    lt->mem_table = NULL;
    lt->imm_size = lt->mem_size;
    lt->mem_size = LSM_TREE_RESET_IN_MEM_SIZE;
    lt->max_k2v_size = 0;
    lt->compact_thread_stop = 1;
    wake_up_interruptible(&lt->waitq);
    kthread_stop(lt->compact_thread);
    memtable_flush(lt);

    kmem_cache_destroy(lt->rb_node_slab);

    for(prev=NULL, cur=rb_first(lt->mem_index); cur; prev=cur, cur=rb_next(cur)){
        rbi = rb_entry(cur, struct y_rb_index, node);
        if(prev)
            rb_entry(prev, struct y_rb_index, node)->nxt = rbi;
    }
    rb_entry(prev, struct y_rb_index, node)->nxt = NULL;
    for(rbi = rb_entry(rb_first(lt->mem_index), struct y_rb_index, node), tmp=rbi->nxt; tmp;rbi=tmp, tmp=rbi->nxt){
        kfree(rbi);
    }
    kfree(rbi);

    kvfree(lt->hp.arr);
    
    mutex_destroy(&lt->hp_lk);

    kvfree(lt->comp_buf);
    kvfree(lt->read_buf);

    write_unlock(&lt->ext_lk);
}