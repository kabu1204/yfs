#include "bloom_filter.h"
#include "lsmtree.h"
#include "mem_index.h"
#include "phys_io.h"
#include "types.h"
#include "heap.h"
#include "merge.h"
#include <linux/slab.h>
#include <linux/mm.h>

#define EXT_SORT_MAX_K 10

struct kmem_cache* k2v_list_slab = NULL;

void merge_init(){
    k2v_list_slab = kmem_cache_create("k2v_list_slab", sizeof(struct y_k2v_list_node), 0, SLAB_HWCACHE_ALIGN, NULL);
}

int y_k2v_less(const void* left, const void* right){
    struct y_k2v* l = *(struct y_k2v**)left;
    struct y_k2v* r = *(struct y_k2v**)right;
    return y_key_cmp(&l->key, &r->key)<0;
}

void y_k2v_swap(void* left, void* right){
    struct y_k2v* t = *(struct y_k2v**)left;
    *(struct y_k2v**)left = *(struct y_k2v**)right;
    *(struct y_k2v**)right = t;
}

void merge_flush(struct lsm_tree* lt, struct y_k2v_list_node* head, struct y_rb_index* old, unsigned int n_rbi){
    struct y_k2v* k2v;
    struct y_k2v_list_node *cur, *prev;
    struct bloom_filter* bfs[Y_DATA_BLOCK_PER_TABLE];
    struct y_rb_index **indices, *rbi;
    unsigned int k2v_size, prev_size;
    unsigned int off = 0;
    unsigned int basep;
    unsigned int i, i_block, i_rbi, n_table;
    unsigned int metap = Y_META_BLOCK_HEADER_SIZE;
    int start = 0;
    char *buf;
    buf = lt->comp_buf;

    prev_size = k2v_size = n_table = 0;
    metap = Y_META_BLOCK_HEADER_SIZE;
    i_rbi = -1;
    buf = lt->comp_buf;
    indices = kvmalloc(LSM_TREE_FLUSH_PER_COMPACT*Y_DATA_BLOCK_PER_TABLE*sizeof(void*), GFP_KERNEL);

    for(i=0;i<Y_DATA_BLOCK_PER_TABLE;++i){
        bfs[i] = bloom_alloc();
    }

    cur = head;
    while(cur){
        basep = 0;
        off = Y_BLOCK_SIZE;
        i_block = -1;
        start = 0;
        metap = Y_META_BLOCK_HEADER_SIZE;
        k2v = &cur->k2v;
        for(prev=NULL; cur; prev=cur, cur=cur->next){
            // while(cur && cur->k2v.ptr.page_no==OBJECT_DEL){
            //     cur = cur->next;
            // }
            k2v = &cur->k2v;
            k2v_size = lsm_k2v_size(&k2v->key);
            if(unlikely(off + k2v_size > Y_BLOCK_SIZE)){
                if(likely(prev)){
                    memcpy(buf+metap, buf+basep+off-prev_size, prev_size);  // dump end_key of i_block
                    basep += Y_BLOCK_SIZE;
                    indices[i_rbi]->end = prev->k2v.key;
                }
                if(off<Y_BLOCK_SIZE){
                    memset(buf+basep+off, 0, Y_BLOCK_SIZE-off);
                }
                if(unlikely(i_block==Y_DATA_BLOCK_PER_TABLE-1)){
                    break;
                }
                memset(bfs[++i_block], 0, sizeof(struct bloom_filter));
                indices[++i_rbi] = kzalloc(sizeof(struct y_rb_index), GFP_KERNEL);
                indices[i_rbi]->start = k2v->key;
                metap += Y_MAX_K2V_SIZE;
                start = 1;
                off = 0;
            }
            bloom_add(bfs[i_block], &k2v->key);
            if(unlikely(k2v_size != dump_k2v(buf+basep+off, &k2v->key, k2v->ptr, k2v->timestamp))) {
                pr_err("[compact] k2v_size unmatch\n");
            }
            if(unlikely(start)){
                memcpy(buf+metap, buf+basep+off, k2v_size);
                metap += Y_MAX_K2V_SIZE;
                start = 0;
            }
            off += k2v_size;
            prev_size = k2v_size;
        }

        memcpy(buf, buf+Y_BLOCK_SIZE, lsm_k2v_size(&head->k2v.key));   // table start_key
        memcpy(buf, buf+basep+off-prev_size, prev_size);
        memcpy(buf+metap, buf, prev_size);  // table end_key
        metap += Y_MAX_K2V_SIZE;

        for(i=0;i<=i_block;++i){
            memcpy(buf+metap, bfs[i], sizeof(struct bloom_filter));
            metap += sizeof(struct bloom_filter);
            indices[i]->blk.table_no = lt->nr_l1 + n_table;
            indices[i]->blk.block_no = i;
        }

        pr_info("[compact] dump bloom filters finished, metap = %uKB\n", metap>>10);

        yssd_write_phys_pages(buf, lt->p1, Y_TABLE_SIZE>>Y_PAGE_SHIFT);

        pr_info("[compact] p1 moved from %u to %lu\n", lt->p1, lt->p1 + (Y_TABLE_SIZE>>Y_PAGE_SHIFT));
        lt->p1 += Y_TABLE_SIZE>>Y_PAGE_SHIFT;
        ++n_table;
    }

    write_lock(&lt->index_lk);
    for(i=0; i<=i_rbi; ++i){
        y_rbi_insert(lt->mem_index, indices[i]);
    }
    lt->mem_index_nr += i_rbi+1;
    for(i=0;i<LSM_TREE_FLUSH_PER_COMPACT*Y_DATA_BLOCK_PER_TABLE;++i){
        rbi = y_rbi_find(lt->mem_index, &old[i]);
        rb_erase(&rbi->node, lt->mem_index);
        kfree(rbi);
        --lt->mem_index_nr;
    }
    lt->nr_l1 += n_table;
    lt->nr_l0 = 0;
    lt->p0 = LSM_TREE_LEVEL0_START_PAGE;
    write_unlock(&lt->index_lk);

    for(i=0;i<Y_DATA_BLOCK_PER_TABLE;++i){
        kfree(bfs[i]);
    }
    kvfree(indices);
}

/*
    merge k continuous tables on disk.
*/
int merge_table(struct lsm_tree* lt, unsigned int k){
    struct min_heap h;
    struct y_k2v_list_node* heads;
    struct y_k2v_list_node* cur, *prev, *out, *t;
    struct y_rb_index old[LSM_TREE_FLUSH_PER_COMPACT*Y_DATA_BLOCK_PER_TABLE];
    char *buf;
    unsigned int p, basep;
    unsigned int size;
    unsigned int i, i_block, i_rbi, n_block;
    buf = lt->comp_buf;

    if(k>EXT_SORT_MAX_K){
        pr_err("[ext sort] too many files: %u, max: %u", k, EXT_SORT_MAX_K);
        return -1;
    }

    p = 0;
    i_rbi = 0;

    h.len = k;
    h.cap = k;
    h.size = sizeof(void*);
    h.arr = kzalloc(k*h.size, GFP_KERNEL);
    h.swap = y_k2v_swap;
    h.less = y_k2v_less;

    heads = kvmalloc(k*sizeof(struct y_k2v_list_node), GFP_KERNEL);
    for(i=0;i<LSM_TREE_FLUSH_PER_COMPACT;++i){
        yssd_read_phys_pages(buf, LSM_TREE_LEVEL0_START_PAGE+i*(Y_TABLE_SIZE>>Y_PAGE_SHIFT), Y_TABLE_SIZE>>Y_PAGE_SHIFT);
        cur = &heads[i];
        cur->next = prev = NULL;
        n_block = *(unsigned int*)(buf+Y_MAX_K2V_SIZE*2);
        for(i_block=0;i_block<n_block;++i_block, ++i_rbi){
            p = 0;
            basep = (i_block+1)*Y_BLOCK_SIZE;
            old[i_rbi].blk.table_no = i;
            old[i_rbi].blk.block_no = i_block;
            while(p<Y_BLOCK_SIZE && (size = read_k2v(buf+basep+p, &cur->k2v))!=0){
                if(unlikely(p==0)) old[i].start = cur->k2v.key;
                prev = cur;
                cur->next = kmem_cache_alloc(k2v_list_slab, GFP_KERNEL);
                cur = cur->next;
                p += size;
            }
            old[i_rbi].end = prev->k2v.key;
            prev->next=NULL;
            kmem_cache_free(k2v_list_slab, cur);
        }
        min_heap_push(&h, &heads[i]);
    }

    out = kmalloc(sizeof(struct y_k2v_list_node), GFP_KERNEL);
    cur = out;
    while(!min_heap_empty(&h)){
        prev = cur;
        cur = *(struct y_k2v_list_node**)min_heap_min(&h);
        if(cur->next)
            min_heap_replace_min(&h, cur->next);
        else
            min_heap_pop(&h);
        while(!min_heap_empty(&h)){
            t = *(struct y_k2v_list_node**)min_heap_min(&h);
            if(t->next)
                min_heap_replace_min(&h, t->next);
            else
                min_heap_pop(&h);
            if(y_key_cmp(&cur->k2v.key, &t->k2v.key)==0){
                if(t->k2v.timestamp > cur->k2v.timestamp){
                    kmem_cache_free(k2v_list_slab, cur);
                    cur = t;
                } else {
                    kmem_cache_free(k2v_list_slab, t);
                    continue;
                }
            } else {
                break;
            }
        }
        prev->next = cur;
    }

    merge_flush(lt, out->next, old, i_rbi+1);

    for(prev=out, cur = prev->next; prev; prev=cur, cur = prev->next){
        kmem_cache_free(k2v_list_slab, prev);
    }

    kfree(h.arr);
    kvfree(heads);
    return 1;
}
