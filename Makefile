PWD := $(CURDIR)

YSSD := $(PWD)/yssd

YFS := $(PWD)/yfs

export CC

all:
	make -C $(YSSD)
	make -C $(YFS)

clean:
	make -C $(YSSD) clean
	make -C $(YFS) clean