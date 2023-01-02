#include "../yssd/yssd_ioctl.h"
#include "../yssd/yssd.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <string.h>

struct y_io_req req;
struct y_key key;
#define YSSD_DEV "/dev/yssd_disk0"
#define V_BUF_LEN 4096
#define DEL_OR_NOT_FOUND(e) (e==ERR_DELETED || e==ERR_NOT_FOUND)

void setup_key(char typ, unsigned int ino, const char* name){
    key.typ = typ;
    key.ino = ino;
    strcpy(key.name, name);
    key.len = strlen(name);
}

void setup_val(const char* val, unsigned int len){
    memcpy(req.val.buf, val, len);
    req.val.len = len;
}

int main(){
    int res, n, fd;
    char buf[sizeof(struct y_key)+24];
    
    fd = open(YSSD_DEV, O_RDONLY);
    if(fd==-1){
        perror("failed to open yssd dev\n");
        return -1;
    }

    req.key = &key;
    req.val.buf = malloc(V_BUF_LEN);

    // 1. SET("m:123:bob.txt", "hello, yssd.")
    setup_key('m', 123, "bob.txt");
    setup_val("hello, yssd\0", 12);

    ioctl(fd, IOCTL_SET, &req);

    // 2. GET("m:123:bob.txt")
    memset(req.val.buf, 0, V_BUF_LEN);
    res = ioctl(fd, IOCTL_GET, &req);
    printf("val: %s\n", req.val.buf);

    // 3. DEL("m:123:bob.txt")
    ioctl(fd, IOCTL_DEL, &req);

    // 4. GET("m:123:bob.txt")
    res = ioctl(fd, IOCTL_GET, &req);
    if(DEL_OR_NOT_FOUND(res)){
        printf("key not found or deleted\n");
    }

    // 5. ITER("m:123")
    setup_key('m', 123, "alice.txt");
    setup_val("alice\0", 6);
    ioctl(fd, IOCTL_SET, &req);

    setup_key('m', 123, "bell.txt");
    setup_val("bell\0", 5);
    ioctl(fd, IOCTL_SET, &req);

    setup_key('m', 123, "kevin.txt");
    setup_val("kevin\0", 6);
    ioctl(fd, IOCTL_SET, &req);

    req.ino = 123;
    for(res=ioctl(fd, IOCTL_ITER, &req); res!=ERR_NOT_FOUND; res=ioctl(fd, IOCTL_NEXT, &req)){
        sprint_y_key(buf, &key);
        if(res==ERR_DELETED){
            printf("[ITER] deleted key=%s\n", buf);
            continue;
        }
        printf("[ITER] (k, v)=(%s, %s)\n", buf, req.val.buf);
    }

    close(fd);
}