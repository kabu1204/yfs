#ifndef YSSD_TYPES_H
#define YSSD_TYPES_H

#include <linux/printk.h>
#include <asm/page_types.h>
#include <linux/fs.h>
#include <linux/slab.h>
#include "yssd.h"

#ifndef SECTOR_SIZE
#define SECTOR_SIZE 512
#endif

#define Y_PAGE_SHIFT PAGE_SHIFT
#define Y_PAGE_SIZE  PAGE_SIZE          // 4KB minimum r/w unit
#define Y_BLOCK_SIZE (Y_PAGE_SIZE<<4)   // 64KB
#define Y_NR_BLOCK_PER_TABLE 32         // 1 metablock + 31 datablock
#define Y_MAX_K2V_SIZE       256
#define Y_DATA_BLOCK_PER_TABLE (Y_NR_BLOCK_PER_TABLE-1)
#define Y_META_BLOCK_HEADER_SIZE (0b11<<9) // 1.5KB
#define Y_META_BLOCK_BF_OFFSET   (Y_META_BLOCK_HEADER_SIZE+Y_DATA_BLOCK_PER_TABLE*2*Y_MAX_K2V_SIZE)
#define Y_TABLE_SIZE (Y_BLOCK_SIZE*Y_NR_BLOCK_PER_TABLE)  // 2MB
#define Y_TABLE_DATA_SIZE  (Y_BLOCK_SIZE*Y_DATA_BLOCK_PER_TABLE)    // 31*64KB
#define Y_VLOG_FLUSH_SIZE  (1ul<<23)      // 8MB
#define SMALL_OBJECT_SIZE  Y_PAGE_SIZE

#define Y_KV_SUPERBLOCK ('s')
#define Y_KV_META       ('m')
#define Y_KV_DATA       ('d')

#define NR_RESERVED_PAGE 3

#define SUPER_BLOCK_PAGE 0
#define TXN_META_PAGE    1
#define NR_RESERVED_PAGE 3

#define OBJECT_NOT_FOUND  0
#define OBJECT_DEL         1
#define OBJECT_VAL_UNFLUSH 2
#define Y_RESERVED_PAGES  OBJECT_VAL_UNFLUSH+1

int y_key_cmp(struct y_key *left, struct y_key *right);
unsigned long sdbm_hash(const unsigned char *str);
unsigned long y_key_hash(struct y_key* key);

inline unsigned long align_backward(unsigned long x, unsigned int shift);

#endif