#ifndef YSSD_TYPES_H
#define YSSD_TYPES_H

#include <linux/printk.h>
#include <asm/page_types.h>

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif

#define Y_PAGE_SIZE  PAGE_SIZE          // 4KB minimum r/w unit
#define Y_BLOCK_SIZE (Y_PAGE_SIZE<<3)   // 64KB
#define Y_TABLE_SIZE (Y_BLOCK_SIZE*32)  // 2MB

#define Y_KV_SUPERBLOCK ('s')
#define Y_KV_META       ('m')
#define Y_KV_DATA       ('d')

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
    char typ;
    unsigned int ino;
    char name[250];
};

struct y_value {
    unsigned int len;
    char *buf;
};

struct y_val_ptr {
    unsigned int page_no;
    unsigned int len;
    unsigned int off;
};

struct y_k2v {
    struct y_key key;
    struct y_val_ptr ptr;
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

#define print_y_key(key) {pr_info("%c:%d%s%s\n", key->typ, key->ino, (key->name[0]!='\0'?":":""), key->name);}
#define sprint_y_key(buf, key) {snprintf(buf, sizeof(struct y_key)+24, "%s:%d%s%s", key->typ==METADATA?"m":"d", key->ino, (key->name[0]!='\0'?":":""), key->name);}

int y_key_cmp(struct y_key *left, struct y_key *right);

#endif