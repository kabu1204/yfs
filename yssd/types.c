#include "types.h"
#include <linux/string.h>

/* 
  -1: left < right 
   0: left == right
   1: left > right
*/
int y_key_cmp(struct y_key *left, struct y_key *right)
{
    if(left->typ!=right->typ) return (left->typ<right->typ)?-1:1;
    if(left->ino!=right->ino) return (left->ino<right->ino)?-1:1;
    return strcmp(left->name, right->name);
}