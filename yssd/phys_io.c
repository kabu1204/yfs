#include "phys_io.h"
#include <linux/fs.h>

extern struct file* fp;
extern unsigned long n_pages;

void yssd_read_phys_page(char* buf, unsigned long page_no){
    loff_t off=page_no*PAGE_SIZE;
    size_t sz;
    if(likely(page_no<n_pages)){
        sz = kernel_read(fp, buf, Y_PAGE_SIZE, &off);
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
        sz = kernel_write(fp, buf, Y_PAGE_SIZE, &off);
        if(unlikely(sz != PAGE_SIZE)){
            pr_warn("kernel_read %ld bytes != PAGE_SIZE\n", sz);
        }
        return;
    } else {
        pr_warn("page_no %ld >= n_pages(%ld)\n", page_no, n_pages);
    }
}

void yssd_read_phys_pages(char* buf, unsigned long page_no, unsigned long npages){
    loff_t off=page_no*PAGE_SIZE;
    size_t sz;
    if(likely(page_no+npages<=n_pages)){
        sz = kernel_read(fp, buf, npages*Y_PAGE_SIZE, &off);
        if(unlikely(sz & (Y_PAGE_SIZE-1))){
            pr_warn("kernel_read %ld bytes != N*PAGE_SIZE\n", sz);
        }
        return;
    } else {
        pr_warn("page_no+npages(%lu+%lu) > n_pages(%ld)\n", page_no, npages, n_pages);
    }
}

void yssd_write_phys_pages(char* buf, unsigned long page_no, unsigned long npages){
    loff_t off=page_no*PAGE_SIZE;
    size_t sz;
    if(likely(page_no+npages<=n_pages)){
        sz = kernel_write(fp, buf, npages*Y_PAGE_SIZE, &off);
        if(unlikely(sz & (Y_PAGE_SIZE-1))){
            pr_warn("kernel_write %ld bytes != N*PAGE_SIZE\n", sz);
        }
        return;
    } else {
        pr_warn("page_no+npages(%lu+%lu) > n_pages(%ld)\n", page_no, npages, n_pages);
    }
}