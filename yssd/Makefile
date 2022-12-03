MODULE = yssd

obj-m += yssd.o
$(MODULE)-objs := module.o rbkv.o types.o

PWD := $(CURDIR)

all:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) modules 
 
clean:
	make -C /lib/modules/$(shell uname -r)/build M=$(PWD) clean
