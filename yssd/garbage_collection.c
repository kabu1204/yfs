#include "lsmtree.h"
#include "phys_io.h"
#include "types.h"
#include "value_log.h"
#include "linux/slab.h"

extern unsigned long n_pages;

static unsigned long gc_times = 0;
static unsigned long gc_stat[N_TOTAL_ITEMS];
static unsigned long gc_stat_each[N_TOTAL_ITEMS];

inline void full_free_vlnode(struct vlog_list_node* vlnode, struct kmem_cache* vlist_slab){
    kzfree(vlnode->vnode.v.buf);
    kmem_cache_free(vlist_slab, vlnode);
}

void init_gc_stat(void){
    gc_times = 0;
    memset(gc_stat_each, 0, sizeof(gc_stat_each));
    memset(gc_stat, 0, sizeof(gc_stat));
}

inline void clear_gc_stat_each(void){
    memset(gc_stat_each, 0, sizeof(gc_stat_each));
}

inline void acc_gc_stat(void){
    int i=0;
    for(i=0;i<N_TOTAL_ITEMS;++i){
        gc_stat[i] += gc_stat_each[i];
    }
}

inline void inc_gc_stat_item(unsigned long delta, enum gc_stat_item item){
    gc_stat_each[item] += delta;
}

inline void dump_gc_stat(void){
    pr_info("[GC] NR_ROUND:           %lu\n", gc_stat[NR_ROUND]);
    pr_info("[GC] NR_PAGES_COLLECTED: %lu\n", gc_stat[NR_PAGES_COLLECTED]);
    pr_info("[GC] NR_PAGES_FREED:     %lu\n", gc_stat[NR_PAGES_FREED]);
    pr_info("[GC] NR_VALID_KV:        %lu\n", gc_stat[NR_VALID_KV]);
    pr_info("[GC] TIME_PER_GC_NS:     %lu\n", gc_stat[TIME_PER_GC_NS]);
}

inline void dump_gc_stat_each(void){
    pr_info("[GC] EACH_NR_ROUND:           %lu\n", gc_stat_each[NR_ROUND]);
    pr_info("[GC] EACH_NR_PAGES_COLLECTED: %lu\n", gc_stat_each[NR_PAGES_COLLECTED]);
    pr_info("[GC] EACH_NR_PAGES_FREED:     %lu\n", gc_stat_each[NR_PAGES_FREED]);
    pr_info("[GC] EACH_NR_VALID_KV:        %lu\n", gc_stat_each[NR_VALID_KV]);
    pr_info("[GC] EACH_TIME_PER_GC_NS:     %lu\n", gc_stat_each[TIME_PER_GC_NS]);
    pr_info("[GC] EACH_TIME_PER_GC_MS:     %lu\n", gc_stat_each[TIME_PER_GC_NS]/1000000);
}

void vlog_flush_gc(struct value_log* vlog, struct list_head* vlnodes, char *buf){
    unsigned int p=0;
    unsigned int npages;
    unsigned int tail;
    unsigned long off;
    struct lsm_tree* lt;
    struct vlog_list_node* vlnode;
    struct list_head *cur, *tmp, *prev=NULL;
    struct kmem_cache* vl_slab;
    struct y_val_ptr ptr;
    vl_slab = vlog->vl_slab;
    lt = vlog->lt;

    while(!list_empty(vlnodes)){
        p = 0;
        // 1. dump to buf
        list_for_each(cur, vlnodes){
            vlnode = container_of(cur, struct vlog_list_node, lhead);
            if(unlikely(p + 4 + vlog_dump_size(&vlnode->vnode.key, &vlnode->vnode.v)>Y_VLOG_FLUSH_SIZE)){
                break;
            }
            vlnode->vnode.offset = p;
            p += vlog_node_dump(&vlnode->vnode, buf+p);
        }

        // 2. persist to disk
        p = align_backward(p+4, Y_PAGE_SHIFT);
        npages = p >> Y_PAGE_SHIFT;
        *(unsigned int*)(&buf[p-4]) = npages;

        if(vlog->tail2 - vlog->head>=npages){
            vlog->tail2 -= npages;
            tail = vlog->tail2;
            pr_info("[GC flush] tail2 moved from %u to %u\n", tail+npages, tail);
        } else {
            vlog->tail1 -= npages;
            tail = vlog->tail1;
            pr_info("[GC flush] tail1 moved from %u to %u\n", tail+npages, tail);
        }
        // TODO: consider order
        yssd_write_phys_pages(buf, tail+1, npages);
        memset(buf, 0, p);
        prev = cur;

        // 3. update list
        list_for_each_safe(cur, tmp, vlnodes){
            if(cur==prev) break;
            vlnode = container_of(cur, struct vlog_list_node, lhead);
            off = vlnode->vnode.offset;
            ptr.page_no = tail + 1 + (off >> Y_PAGE_SHIFT);
            ptr.off = off & (Y_PAGE_SIZE-1);
            list_del(cur);
            lsm_tree_get_and_set(lt, &vlnode->vnode.key, ptr, vlnode->vnode.timestamp);
            full_free_vlnode(vlnode, vl_slab);
        }
    }
}

void vlog_gc(struct value_log* vlog){
    unsigned int head;
    unsigned int total_pages, npages, count=0;
    unsigned int nbytes, p=0;
    unsigned int page_no, offset;
    char *buf;
    struct y_val_ptr ptr;
    struct vlog_list_node* vlnode;
    struct kmem_cache* vl_slab;
    struct lsm_tree* lt;
    /*
        stat related
    */
    unsigned long start_time, end_time, round, n_valid, tail1, tail2;

    LIST_HEAD(vlnodes);
    head = vlog->head;
    lt = vlog->lt;
    vl_slab = vlog->vl_slab;

    start_time = ktime_get_ns();
    round = 0;
    n_valid = 0;
    tail1 = vlog->tail1;
    tail2 = vlog->tail2;
    clear_gc_stat_each();

    total_pages = head - vlog->tail1;
    total_pages = total_pages>=VLOG_GC_PAGES ? VLOG_GC_PAGES : total_pages;

    pr_info("[GC] total_pages = %u\n", total_pages);

    buf = vzalloc(Y_VLOG_FLUSH_SIZE);
    yssd_read_phys_page(buf, head);
    nbytes = Y_PAGE_SIZE;

    while(count < total_pages){
        npages = *(unsigned int*)(buf+(nbytes-4));
        count += npages;
        head -= npages;
        nbytes = npages << Y_PAGE_SHIFT;
        p = 0;
        ++round;
        pr_info("[GC Round %lu] npages = %u nbytes = %u\n", round, npages, nbytes);
        yssd_read_phys_pages(buf, head+1, npages);
        while(p+VLOG_RESET_IN_MEM_SIZE<nbytes){
            int n;
            vlnode = kmem_cache_alloc(vl_slab, GFP_KERNEL);
            if(!vlnode){
                pr_err("[GC] slab alloc failed\n");
            }

            n = vlog_node_load(buf+p, &vlnode->vnode);
            if(unlikely(n==0)){
                pr_info("[GC] load end\n");
                kmem_cache_free(vl_slab, vlnode);
                break;
            }

            page_no = head + 1 + (p>>Y_PAGE_SHIFT);
            offset = p & (Y_PAGE_SIZE-1);
            p += n;

            ptr = lsm_tree_get(lt, &vlnode->vnode.key);
            if(ptr.page_no != page_no || ptr.off != offset){
                // deleted or out-of-date value
                full_free_vlnode(vlnode, vl_slab);
                continue;
            }
            ++n_valid;
            list_add_tail(&vlnode->lhead, &vlnodes);
        }
    }

    total_pages = count;

    /*
        1. flush to disk;
        2. move tail1;
        3. update k2v index in lsmtree;
    */
    vlog_flush_gc(vlog, &vlnodes, buf);
    vfree(buf);

    inc_gc_stat_item(round, NR_ROUND);
    inc_gc_stat_item(total_pages, NR_PAGES_COLLECTED);
    inc_gc_stat_item(total_pages-((tail1 - vlog->tail1) + (tail2-vlog->tail2)), NR_PAGES_FREED);
    inc_gc_stat_item(n_valid, NR_VALID_KV);

    write_lock(&vlog->rwlock);
    if(head==vlog->tail1){
        vlog->tail1 = vlog->tail2;
        vlog->head = vlog->tail2 = n_pages-1;
        pr_info("[GC] head catches up with tail1 at %u, reset\n", head);
    } else {
        vlog->head = head;
        pr_info("[GC] head moved to %u\n", head);
    }
    write_unlock(&vlog->rwlock);

    end_time = ktime_get_ns();
    inc_gc_stat_item(end_time - start_time, TIME_PER_GC_NS);
    acc_gc_stat();
    dump_gc_stat_each();
}