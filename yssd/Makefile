MODULE = yssd

obj-m += yssd.o
$(MODULE)-objs := module.o rbkv.o types.o lsmtree.o phys_io.o value_log.o garbage_collection.o kv.o compaction.o bloom_filter.o mem_index.o heap.o

PWD := $(CURDIR)

MY_CFLAGS += -g -DDEBUG
MY_CFLAGS += -msoft-float -msse -msse2 -msse3 -msse4
ccflags-y += ${MY_CFLAGS}
CC += ${MY_CFLAGS}

all:
	make -C $(LINUX_DIR) M=$(PWD) modules 

debug:
	make -C $(LINUX_DIR) M=$(PWD) modules EXTRA_CFLAGS="$(MY_CFLAGS)"

clean:
	make -C $(LINUX_DIR) M=$(PWD) clean
