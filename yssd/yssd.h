#ifndef YSSD_YSSD_H
#define YSSD_YSSD_H

#ifdef __KERNEL__
#include <linux/printk.h>
#else
#include <stdio.h>
#endif

enum y_io_req_type{
    GET,        // GET
    SET,        // SET sync
    DEL,        // DELETE
    ITER,       // ITERATE
    BeginTX,    // BeginTX
    AbortTX,    // AbortTX
    EndTX,      // EndTX

    GET_FIRST_BLOCK,    // first ssd block is used to storage superblock info
};

enum y_txn_status {
    RUNNING,
    ABORTED,
    COMMITED,
    CHECKPOINTED,
};

struct y_key {
    unsigned int ino;
    /*
        for META, stand for the length of name;
        for DATA, stand for the serial number of subobject.
    */
    unsigned int len;
    char typ;
    char name[234];
};

struct y_value {
    unsigned int len;
    char *buf;
};

struct y_val_ptr {
    unsigned int page_no;
    unsigned int off;
};

struct y_k2v {
    struct y_key key;
    struct y_val_ptr ptr;
    unsigned long timestamp;
};

struct y_io_req {
    enum y_io_req_type typ;
    unsigned long tid;
    struct y_key* key;
    union {
        // for GET/SET/DEL
        struct {
            unsigned int off;
            unsigned int len;
        };
        // for ITER
        struct {
            unsigned long ino;
            unsigned int cnt;
        };
    };
    struct y_value val;
};

#ifdef __KERNEL__
#define print_y_key(key) {pr_info("%c:%u%s%s\n", key->typ, key->ino, (key->name[0]!='\0'?":":""), key->name);}
#else
#define print_y_key(key) {printf("%c:%u%s%s\n", key->typ, key->ino, (key->name[0]!='\0'?":":""), key->name);}
#endif

#define sprint_y_key(buf, key) {snprintf(buf, sizeof(struct y_key)+24, "%c:%u%s%s", (key)->typ, (key)->ino, ((key)->name[0]!='\0'?":":""), (key)->name);}

#define ERR_NOT_FOUND 1
#define ERR_DELETED   2

#endif