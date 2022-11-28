#include "skiplist.h"
#include "types.h"
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/random.h>

int rand_lvl()
{
    int lvl = 1;
    char rand;
    get_random_bytes(&rand, sizeof(rand));
    while(rand&1 && lvl<SKIP_LIST_MAX_LVL){
        ++lvl;
        get_random_bytes(&rand, sizeof(rand));
    }
    return lvl-1;
}

struct y_skip_node* y_skip_find(struct skip_list* l, struct y_key* key)
{
    struct skip_node* p = l->head, *q=NULL;
    struct y_skip_node* cur;
    int i;
    for(i=l->lvl; i>=0; --i)
    {
        for(q = p->next[i]; q!=NULL; q = p->next[i])
        {
            cur = container_of(q, struct y_skip_node, node);
            if(unlikely(y_key_cmp(&cur->key, key)>=0)) break;
            p = q;
        }
        if(q!=NULL && y_key_cmp(&cur->key, key)==0){
            return cur;
        }
    }
    return NULL;
}

int y_skip_insert(struct skip_list* l, struct y_skip_node* elem)
{
    struct y_skip_node* cur;
    struct skip_node* upd[SKIP_LIST_MAX_LVL];
    struct skip_node *p=l->head, *q=NULL;
    int i, new_lvl;

    for(i=l->lvl; i>=0; --i)
    {
        for(q = p->next[i]; q!=NULL; q = p->next[i])
        {
            cur = container_of(q, struct y_skip_node, node);
            if(unlikely(y_key_cmp(&cur->key, &elem->key)>=0)) break;
            p = q;
        }
        upd[i] = p;
    }

    if(q!=NULL && y_key_cmp(&cur->key, &elem->key)==0){
        return 0;
    }

    new_lvl = rand_lvl();
    if(new_lvl > l->lvl){
        for(i = l->lvl+1; i <= new_lvl; ++i){
            upd[i] = l->head;
        }
        l->lvl = new_lvl;
    }

    q = &elem->node;

    for(i=new_lvl; i>=0; --i){
        q->next[i] = upd[i]->next[i];
        upd[i]->next[i] = q;
    }
    ++l->size;
    return 0;
}

int skip_erase(struct skip_list* l, struct y_key* key)
{
    struct y_skip_node* cur;
    struct skip_node* upd[SKIP_LIST_MAX_LVL];
    struct skip_node *p=l->head, *q=NULL;
    int i;

    for(i=l->lvl; i>=0; --i)
    {
        for(q = p->next[i]; q!=NULL; q = p->next[i])
        {
            cur = container_of(q, struct y_skip_node, node);
            if(unlikely(y_key_cmp(&cur->key, key)>=0)) break;
            p = q;
        }
        upd[i] = p;
    }

    if(q==NULL || y_key_cmp(&cur->key, key)>0){
        return 0;
    }

    for(i=0; i<l->lvl; ++i){
        upd[i]->next[i] = q->next[i];
    }

    while(l->lvl>0 && l->head->next[l->lvl]==q) --l->lvl;

    kfree(cur);

    --l->size;
    return 0;
}