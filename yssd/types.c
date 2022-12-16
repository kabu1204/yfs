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
    if(left->len!=right->len) return (left->len<right->len)?-1:1;
    return (left->typ=='m')?strcmp(left->name, right->name):0;
}

unsigned long sdbm_hash(const unsigned char *str)
{
  unsigned long hash = 0;
  int c;
  while (c = *str++)
      hash = c + (hash << 6) + (hash << 16) - hash;
  return hash;
}

inline unsigned long y_key_hash(struct y_key* key){
  unsigned long hash=0;
  unsigned long t = key->ino;
  char c;
  char *p = key->name;
  t <<= 32;
  t |= key->len;
  t = (t ^ (t >> 30)) * 0xbf58476d1ce4e5b9ul;
  t = (t ^ (t >> 27)) * 0x94d049bb133111ebul;
  t = t ^ (t >> 31);

  hash = key->typ;
  while (c = *p++)
    hash = c + (hash << 6) + (hash << 16) - hash;

  return hash ^ (t + 0x517cc1b727220a95 + (hash << 6) + (hash >> 2));
}

inline unsigned long align_backward(unsigned long x, unsigned int shift){
  unsigned long mask = (1<<shift) - 1;
  if(x & mask)
      x = ((x>>shift)+1)<<shift;
  return x;
}