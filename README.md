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

There are some options as follows:
- `--sort` option is to set the sort order to show.  Its value can be either
  "size" or "count" and its default value is "size".
- `--top` option is to set the number of backtraces to show and its default
  value is 10.

Here is an example usage of heaptrace.  It traces memory allocation of the
target program `node`, then prints currently live allocation info based on
each backtrace of allocation.  It shows that some of the allocated objects are
not deallocated until the program is finished.
```
$ heaptrace --top 3 --sort count /usr/bin/node --expose-gc -e 'gc()'
[heaptrace] initialized for /proc/5879/maps
[heaptrace] finalized for /proc/5879/maps
=================================================================
[heaptrace] dump allocation status for /proc/5879/maps (node)
=== backtrace #1 === [count/peak: 43/60] [size/peak: 22.704 KB/31.680 KB] [age: 6.236 ms]
 0 [0x7fe72e46476c] malloc +0x1b
 1 [0x7fe72dd61e78] operator new(unsigned long) +0x6
 2 [      0xf27811] /usr/bin/node (+0xf27811)
 3 [      0xf2e57d] v8::internal::MarkCompactCollector::RootMarkingVisitor::VisitRootPointer(v8::internal::Root, char const*, v8::internal::Objec... +0x1b
 4 [     0x121c32d] v8::internal::SerializerDeserializer::Iterate(v8::internal::Isolate*, v8::internal::RootVisitor*) +0x47
 5 [      0xf07e45] v8::internal::Heap::IterateStrongRoots(v8::internal::RootVisitor*, v8::internal::VisitMode) +0x91
 6 [      0xf40b6b] v8::internal::MarkCompactCollector::MarkLiveObjects() +0x72
 7 [      0xf42071] v8::internal::MarkCompactCollector::CollectGarbage() +0x4

=== backtrace #2 === [count/peak: 12/13] [size/peak: 6.336 KB/6.864 KB] [age: 6.400 ms]
 0 [0x7fe72e46476c] malloc +0x1b
 1 [0x7fe72dd61e78] operator new(unsigned long) +0x6
 2 [      0xf27811] /usr/bin/node (+0xf27811)
 3 [      0xf29dd4] v8::internal::MarkCompactCollector::RootMarkingVisitor::VisitRootPointers(v8::internal::Root, char const*, v8::internal::Obje... +0x29
 4 [      0xb27068] v8::internal::HandleScopeImplementer::IterateThis(v8::internal::RootVisitor*) +0x1e
 5 [      0xf07d33] v8::internal::Heap::IterateStrongRoots(v8::internal::RootVisitor*, v8::internal::VisitMode) +0x4c
 6 [      0xf40b6b] v8::internal::MarkCompactCollector::MarkLiveObjects() +0x72
 7 [      0xf42071] v8::internal::MarkCompactCollector::CollectGarbage() +0x4

=== backtrace #3 === [count/peak: 10/10] [size/peak: 5.280 KB/5.280 KB] [age: 6.322 ms]
 0 [0x7fe72e46476c] malloc +0x1b
 1 [0x7fe72dd61e78] operator new(unsigned long) +0x6
 2 [      0xf27811] /usr/bin/node (+0xf27811)
 3 [      0xf2e57d] v8::internal::MarkCompactCollector::RootMarkingVisitor::VisitRootPointer(v8::internal::Root, char const*, v8::internal::Objec... +0x1b
 4 [      0xf07f08] v8::internal::Heap::IterateStrongRoots(v8::internal::RootVisitor*, v8::internal::VisitMode) +0xc2
 5 [      0xf40b6b] v8::internal::MarkCompactCollector::MarkLiveObjects() +0x72
 6 [      0xf42071] v8::internal::MarkCompactCollector::CollectGarbage() +0x4
 7 [      0xf15621] v8::internal::Heap::MarkCompact() +0x28

[heaptrace] heap traced num of backtrace : 64
[heaptrace] heap traced allocation size  : 59.215 KB
[heaptrace] allocator info (virtual)     : 2.121 MB
[heaptrace] allocator info (resident)    : 414.288 KB
[heaptrace] statm info (VSS/RSS/shared)  : 134.201 MB / 40.960 KB / 0 bytes
=================================================================
```

It can also dump allocation status when it receives a signal as follows:
- `SIGUSR1`: dump the current allocation status with sort order by "size"
- `SIGUSR2`: dump the current allocation status with sort order by "count"

It can be useful for long running programs.
