#ifndef YSSD_TYPES_H
#define YSSD_TYPES_H

#include <linux/printk.h>

enum y_key_type {
    METADATA,
    DATA
};

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
    enum y_key_type typ;
    unsigned int ino;
    char name[120];
};

struct y_io_req {
    enum y_io_req_type typ;
    unsigned long tid;
    union {
        // for GET/SET/DEL
        struct {
            struct y_key* key;
            unsigned int off;
            unsigned int len;
        };
        // for ITER
        struct {
            unsigned long ino;
            unsigned int cnt;
        };
    };
};

#define print_y_key(key) {pr_info("%s:%d%s%s\n", key->typ==METADATA?"m":"d", key->ino, (key->name[0]!='\0'?":":""), key->name);}
#define sprint_y_key(buf, key) {snprintf(buf, sizeof(struct y_key)+24, "%s:%d%s%s", key->typ==METADATA?"m":"d", key->ino, (key->name[0]!='\0'?":":""), key->name);}

int y_key_cmp(struct y_key *left, struct y_key *right);

#endif