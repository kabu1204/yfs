# yfs

An simple implementation of KEVIN.

Two parts: yfs in filesystem layer and yssd in block device layer.

yfs is responsible for translating POSIX syscalls to KV commands.

yssd is responsible for indexing KV objects and transaction management.

# overview

![](./docs/assets/overview.jpg)

# yssd

![](./docs/assets/yssd.jpg)