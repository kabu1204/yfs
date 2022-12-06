#include "lsmtree.h"
#include "phys_io.h"
#include "types.h"
#include "value_log.h"
#include "linux/slab.h"

inline void full_free_vlnode(struct vlog_list_node* vlnode, struct kmem_cache* vlist_slab){
    kfree(vlnode->vnode.v->buf);
    kfree(vlnode->vnode.v);
    kmem_cache_free(k2v_slab, vlnode->vnode.k2v);
    kmem_cache_free(vlist_slab, vlnode);
}

void vlog_flush_gc(struct value_log* vlog, struct list_head* vlnodes){
    unsigned int p=0;
    unsigned int npages;
    unsigned int tail;
    unsigned long off;
    char *buf;
    struct lsm_tree* lt;
    struct vlog_list_node* vlnode;
    struct list_head *cur, *tmp, *prev=NULL;
    struct kmem_cache* vlist_slab;
    buf = kzalloc(Y_VLOG_FLUSH_SIZE, GFP_KERNEL);
    vlist_slab = vlog->vlist_slab;
    lt = vlog->lt;

    while(!list_empty(vlnodes)){
        p = 0;
        // 1. dump to buf
        list_for_each(cur, vlnodes){
            vlnode = container_of(cur, struct vlog_list_node, lhead);
            if(unlikely(p + 4 + vlog_node_dump_size(vlnode->vnode.k2v, vlnode->vnode.v)>Y_VLOG_FLUSH_SIZE)){
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
        } else {
            vlog->tail1 -= npages;
            tail = vlog->tail1;
        }
        // TODO: consider order
        yssd_write_phys_pages(buf, tail+1, npages);
        memset(buf, 0, p);
        prev=cur;

        // 3. update list
        list_for_each_safe(cur, tmp, vlnodes){
            if(cur==prev) break;
            vlnode = container_of(cur, struct vlog_list_node, lhead);
            off = vlnode->vnode.offset;
            vlnode->vnode.k2v->ptr.page_no = tail + 1 + (off >> Y_PAGE_SHIFT);
            vlnode->vnode.k2v->ptr.off = off & (Y_PAGE_SIZE-1);
            list_del(cur);
            lsm_tree_set(lt, vlnode->vnode.k2v);
            full_free_vlnode(vlnode, vlist_slab);
        }
    }

    kzfree(buf);
}

void vlog_gc(struct value_log* vlog){
    unsigned int head;
    unsigned int total_pages, npages, count=0;
    unsigned int nbytes, p=0;
    unsigned int page_no, offset;
    struct y_val_ptr ptr;
    char *buf;
    struct vlog_list_node* vlnode;
    struct kmem_cache* vlist_slab;
    struct lsm_tree* lt;
    LIST_HEAD(vlnodes);
    head = vlog->head;
    lt = vlog->lt;
    vlist_slab = vlog->vlist_slab;

    total_pages = head - vlog->tail1;
    total_pages = total_pages>=VLOG_GC_PAGES ? VLOG_GC_PAGES : total_pages;

    buf = kmalloc(Y_VLOG_FLUSH_SIZE, GFP_KERNEL);
    yssd_read_phys_page(buf, head);
    nbytes = Y_PAGE_SIZE;

    while(count < total_pages){
        npages = *(unsigned int*)(buf+(nbytes-4));
        count += npages;
        head -= npages;
        nbytes = npages << Y_PAGE_SHIFT;
        p = 0;
        yssd_read_phys_pages(buf, head+1, npages);
        while(p+VLOG_RESET_IN_MEM_SIZE<nbytes){
            int n;
            vlnode = kmem_cache_alloc(vlog->vlist_slab, GFP_KERNEL);

            n = vlog_node_load(buf+p, &vlnode->vnode);
            if(unlikely(n==0)){
                kmem_cache_free(vlog->vlist_slab, vlnode);
                break;
            }

            page_no = head + (p>>Y_PAGE_SHIFT);
            offset = p & (Y_PAGE_SIZE-1);
            p += n;

            ptr = lsm_tree_get(lt, &vlnode->vnode.k2v->key);
            if(ptr.page_no != page_no && ptr.off != offset){
                // deleted or out-of-date value
                full_free_vlnode(vlnode, vlist_slab);
                continue;
            }

            vlnode->vnode.k2v->ptr.timestamp = ptr.timestamp;
            list_add_tail(&vlnode->lhead, &vlnodes);
        }
    }

    total_pages = count;

    /*
        1. flush to disk;
        2. move tail1;
        3. update k2v index in lsmtree;
    */
    vlog_flush_gc(vlog, &vlnodes);

    if(head==vlog->tail1){
        vlog->tail1 = vlog->tail2;
        vlog->head = vlog->tail2 = n_pages-1;
    } else {
        vlog->head = head;
    }
}