/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTRACE_STACKTRACE_H
#define HEAPTRACE_STACKTRACE_H

#include <execinfo.h>

#include <algorithm>
#include <thread>
#include <vector>
#include <map>

#include "heaptrace.h"

#define NUM_BACKTRACE     8
#define NUM_TOP_BACKTRACE 10

using stack_trace_t = std::array<void*, NUM_BACKTRACE>;
using addr_t = void*;

struct stack_info_t {
	size_t total_size;
	size_t stack_depth;
	size_t count;
};

struct object_info_t {
	stack_trace_t stack_trace;
	size_t size;
};

void __record_backtrace(size_t size, void* addr,
			stack_trace_t& stack_trace, int nptrs);

// This is defined as a inline function to avoid having one more useless
// backtrace in the recorded stacktrace.
// Most of the work will be done inside __record_backtrace().
inline void record_backtrace(size_t size, void* addr)
{
	int nptrs;
	stack_trace_t stack_trace = { 0 };

	if (addr == NULL)
		return;

	nptrs = backtrace(stack_trace.data(), NUM_BACKTRACE);
	__record_backtrace(size, addr, stack_trace, nptrs);
}

void release_backtrace(void* addr);

void dump_stackmap(void);

#endif /* HEAPTRACE_STACKTRACE_H */
