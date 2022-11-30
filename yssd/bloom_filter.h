#ifndef YSSD_BLOOM_FILTER_H
#define YSSD_BLOOM_FILTER_H

#define Y_BLOOM_K 3
#define Y_BLOOM_N 2048  // about 2048 K2V in a block
#define Y_BLOOM_F 4     // m/n
#define Y_BLOOM_M Y_BLOOM_F*Y_BLOOM_N
#define Y_BLOOM_SIZE_BYTES Y_BLOOM_M >> 3

struct bloom_filter{
    char array[Y_BLOOM_SIZE_BYTES];
};

struct bloom_filter* bloom_alloc(void);

void bloom_add(struct bloom_filter* bf, const char* key);

int bloom_contains(struct bloom_filter* bf, const char* key);

#endif