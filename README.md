# yfs

A simple implementation of KEVIN.

Two parts: yfs in filesystem layer and yssd in block device layer.

yfs is responsible for translating POSIX syscalls to KV commands.

yssd is responsible for indexing KV objects and transaction management.

# overview

![](./docs/assets/overview.jpg)

# YSSD

![](./docs/assets/yssd.jpg)

## Disk layout

![](./docs/assets/disk_layout.jpg)

## Table layout

![](./docs/assets/table_layout.jpg)

### Meta block

![](./docs/assets/meta_block.jpg)

## Page layout

![](./docs/assets/page_layout.jpg)

## K2V index

![](./docs/assets/k2v_index.jpg)