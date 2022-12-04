#include "phys_io.h"
#include <linux/fs.h>

void yssd_read_phys_page(char* buf, unsigned long page_no){
    loff_t off=page_no*PAGE_SIZE;
    size_t sz;
    if(likely(page_no<n_pages)){
        sz = kernel_read(fp, buf, PAGE_SIZE, &off);
        if(unlikely(sz != PAGE_SIZE)){
            pr_warn("kernel_read %ld bytes != PAGE_SIZE\n", sz);
        }
        return;
    } else {
        pr_warn("page_no %ld >= n_pages(%ld)\n", page_no, n_pages);
    }
}

void yssd_write_phys_page(char* buf, unsigned long page_no){
    loff_t off=page_no*PAGE_SIZE;
    size_t sz;
    if(likely(page_no<n_pages)){
        sz = kernel_write(fp, buf, PAGE_SIZE, &off);
        if(unlikely(sz != PAGE_SIZE)){
            pr_warn("kernel_read %ld bytes != PAGE_SIZE\n", sz);
        }
        return;
    } else {
        pr_warn("page_no %ld >= n_pages(%ld)\n", page_no, n_pages);
    }
}