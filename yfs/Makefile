MODULE = yfs

obj-m += yfs.o
$(MODULE)-objs := fs.o inode.o super.o dir.o file.o

PWD := $(CURDIR)

MY_CFLAGS += -g -DDEBUG
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}

all:
	make -C $(LINUX_DIR) M=$(PWD) modules 

debug:
	make -C $(LINUX_DIR) M=$(PWD) modules EXTRA_CFLAGS="$(MY_CFLAGS)"

clean:
	make -C $(LINUX_DIR) M=$(PWD) clean
