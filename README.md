# YFS

A simple implementation of KEVIN.

Two parts: yfs in filesystem layer and yssd in block device layer.

yfs is responsible for translating POSIX syscalls to KV commands.

yssd is responsible for indexing KV objects and transaction management.

---

- [YFS](#yfs)
- [Overview](#overview)
- [YSSD](#yssd)
  - [Disk layout](#disk-layout)
  - [K2V index (LSM Tree)](#k2v-index-lsm-tree)
    - [Table layout](#table-layout)
      - [Meta block](#meta-block)
    - [Page layout](#page-layout)
    - [K2V index](#k2v-index)
  - [Value Log](#value-log)
    - [General Process](#general-process)
    - [Garbage Collection](#garbage-collection)


# Overview

![](./docs/assets/overview.jpg)

# YSSD

![](./docs/assets/yssd.jpg)

## Disk layout

![](./docs/assets/disk_layout.jpg)

## K2V index (LSM Tree)

### Table layout

![](./docs/assets/table_layout.jpg)

#### Meta block

![](./docs/assets/meta_block.jpg)

### Page layout

![](./docs/assets/page_layout.jpg)

### K2V index

![](./docs/assets/k2v_index.jpg)

## Value Log

### General Process

![](./docs/assets/vlog_overview.jpg)

### Garbage Collection

![](./docs/assets/vlog_gc.jpg)

---

How space freed by GC is used?

vlog maintains two queues: queue1(from `head` to `tail1`) and queue2(from `end` to `tail2`).

1. queue1 and queue2 store persisted KVs.
2. Flush will try to append new KVs to queue2 first.
3. GC always happens on queue1.
4. GC will append valid KVs to queue1.


![](./docs/assets/vlog_gc_two_queue1.jpg)

When `head` catches up with `tail1`, in other words, the length of queue1 becomes 0, then queue1 becomes queue2 and queue2 is reset(`tail2=end`):

![](./docs/assets/vlog_gc_two_queue2.jpg)