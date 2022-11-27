PWD := $(CURDIR)

YSSD := $(PWD)/yssd

export CC

all:
	make -C $(YSSD)

clean:
	make -C $(YSSD) clean
