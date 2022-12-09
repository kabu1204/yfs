#ifndef YSSD_TYPES_H
#define YSSD_TYPES_H

#include <linux/printk.h>
#include <asm/page_types.h>
#include <linux/fs.h>
#include <linux/slab.h>

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif

#define Y_PAGE_SHIFT PAGE_SHIFT
#define Y_PAGE_SIZE  PAGE_SIZE          // 4KB minimum r/w unit
#define Y_BLOCK_SIZE (Y_PAGE_SIZE<<3)   // 64KB
#define Y_TABLE_SIZE (Y_BLOCK_SIZE*32)  // 2MB
#define Y_VLOG_FLUSH_SIZE  1ul<<33      // 8MB
#define SMALL_OBJECT_SIZE  Y_PAGE_SIZE

#define Y_KV_SUPERBLOCK ('s')
#define Y_KV_META       ('m')
#define Y_KV_DATA       ('d')

#define NR_RESERVED_PAGE 3

#define SUPER_BLOCK_PAGE 0
#define TXN_META_PAGE    1
#define NR_RESERVED_PAGE 3

#define OBJECT_SMALL_FILE  0
#define OBJECT_DEL         1
#define OBJECT_VAL_UNFLUSH 2

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
    char name[250];
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

struct file* fp;
unsigned long n_sectors;
unsigned long n_bytes;
unsigned long n_pages;

struct kmem_cache* k2v_slab;

#define print_y_key(key) {pr_info("%c:%u%s%s\n", key->typ, key->ino, (key->name[0]!='\0'?":":""), key->name);}
#define sprint_y_key(buf, key) {snprintf(buf, sizeof(struct y_key)+24, "%c:%u%s%s", (key)->typ, (key)->ino, ((key)->name[0]!='\0'?":":""), (key)->name);}

int y_key_cmp(struct y_key *left, struct y_key *right);
unsigned int key_dump_size(struct y_k2v* kv);
unsigned long sdbm_hash(const unsigned char *str);
unsigned long y_key_hash(struct y_key* key);

inline unsigned long align_backward(unsigned long x, unsigned int shift);

#endif