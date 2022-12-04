#ifndef YSSD_PHYS_IO_H
#define YSSD_PHYS_IO_H
#include "types.h"

void yssd_read_phys_page(char* buf, unsigned long page_no);

void yssd_write_phys_page(char* buf, unsigned long page_no);

#endif