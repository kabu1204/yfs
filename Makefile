PWD := $(CURDIR)

YSSD := $(PWD)/yssd

YFS := $(PWD)/yfs

# LINUX_DIR = /lib/modules/$(shell uname -r)/build
LINUX_DIR = /home/ycy/kernel/linux-5.4

export CC LINUX_DIR

all:
	make -C $(YSSD)
	make -C $(YFS)

debug:
	make -C $(YSSD) debug
	make -C $(YFS) debug

clean:
	make -C $(YSSD) clean
	make -C $(YFS) clean