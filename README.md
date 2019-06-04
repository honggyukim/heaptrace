<!--
SPDX-FileCopyrightText: Copyright (c) 2022 LG Electronics Inc.
SPDX-License-Identifier: GPL-2.0
-->

heaptrace
=========
heaptrace is a tool that collects and reports heap allocated memory.

Approach
========
heaptrace uses widely used LD_PRELOAD technique to trace memory allocation of
the target program.

It doesn't explicitly detect memory leak, but it provides heap allocation status
information that might be a useful hint to find memory leaks.

It provides allocation info for each allocation point and it includes:
- `backtrace`   : allocation point and its backtrace (default depth is 8)
- `count`/`peak`: total allocation count and its peak count
- `size`/`peak` : total allocation size and its peak size
- `age`         : the duration time since the backtrace was created
- `summary`     : general memory allocation summary


Build and installation
======================
heaptrace can be simply compiled as follows:
```
$ make
```
The backtrace depth can be changed with a build flag `DEPTH` as follows:
```
# make DEPTH=30
```
Then heaptrace keeps maximum 30 of backtrace depth when tracing.


How to use heaptrace
====================
It provides a convenience wrapper program instread of using LD_PRELOAD
explicitly as follows:
```
$ heaptrace [<program>]
```

It traces memory allocation of the target program and dump the memory allocation
status when program is finished.  If every memory allocation is properly
deallocated, then it doesn't print anything.

Here is an example usage of heaptrace.  It traces memory allocation of the
target program `factorial`, then prints currently live allocation info based on
each backtrace of allocation.
```
$ heaptrace --top 3 ./factorial
[heaptrace] initialized for /proc/31027/maps
[heaptrace] finalized for /proc/31027/maps
=================================================================
[heaptrace] dump allocation status for /proc/31027/maps (factorial)
=== backtrace #1 === [count/peak: 1/1] [size/peak: 10 bytes/10 bytes] [age: 21.74 us]
 0 [0x7f28cd4ec74c] malloc +0x1b
 1 [      0x40086d] fac +0xd
 2 [      0x400881] fac +0x12
 3 [      0x400881] fac +0x12
 4 [      0x400881] fac +0x12
 5 [      0x400881] fac +0x12
 6 [      0x400881] fac +0x12
 7 [      0x400881] fac +0x12

=== backtrace #2 === [count/peak: 1/1] [size/peak: 10 bytes/10 bytes] [age: 29.373 us]
 0 [0x7f28cd4ec898] calloc +0x1e
 1 [      0x400863] fac +0xb
 2 [      0x400881] fac +0x12
 3 [      0x400881] fac +0x12
 4 [      0x400881] fac +0x12
 5 [      0x400881] fac +0x12
 6 [      0x400881] fac +0x12
 7 [      0x400881] fac +0x12

=== backtrace #3 === [count/peak: 1/1] [size/peak: 7 bytes/7 bytes] [age: 103.486 us]
 0 [0x7f28cd4ec74c] malloc +0x1b
 1 [      0x40084e] fac +0x6
 2 [      0x4008a7] main +0x8
 3 [0x7f28cd140830] __libc_start_main +0x3c
 4 [      0x400769] _start +0xa

[heaptrace] heap traced allocation count : 9 times
[heaptrace] heap traced allocation size  : 48 bytes
[heaptrace] allocator info (virtual)     : 204.800 KB
[heaptrace] allocator info (resident)    : 75.968 KB
[heaptrace] statm info (VSS/RSS/shared)  : 134.205 MB / 36.864 KB / 0 bytes
=================================================================
```
