#include "inode.h"

struct inode* yfs_get_inode(struct super_block* sb, unsigned int ino)
{
    return new_inode(sb);
}