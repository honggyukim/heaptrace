/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTRACE_STACKTRACE_H
#define HEAPTRACE_STACKTRACE_H

#include <execinfo.h>
#include <stdint.h>

#include <array>
#include <chrono>
#include <map>

#include "heaptrace.h"
#include "compiler.h"

extern struct opts opts;

using stack_trace_t = std::array<void*, DEPTH>;
using addr_t = void*;
using time_point_t = std::chrono::steady_clock::time_point;

struct stack_info_t {
	size_t stack_depth;
	uint64_t total_size;
	uint64_t peak_total_size;
	size_t count;
	size_t peak_count;
	time_point_t birth_time;
};

struct object_info_t {
	stack_trace_t stack_trace;
	uint64_t size;
};

extern std::map<addr_t, object_info_t> addrmap;

void __record_backtrace(size_t size, void* addr,
			stack_trace_t& stack_trace, int nptrs);

// This is defined as a inline function to avoid having one more useless
// backtrace in the recorded stacktrace.
// Most of the work will be done inside __record_backtrace().
inline void record_backtrace(size_t size, void* addr)
{
	int nptrs;
	stack_trace_t stack_trace = { 0, };

	if (unlikely(!addr))
		return;

	nptrs = backtrace(stack_trace.data(), DEPTH);
	__record_backtrace(size, addr, stack_trace, nptrs);
}

void release_backtrace(void* addr);

void dump_stackmap(const char *sort_str, bool flamegraph = false);

void clear_stackmap(void);

#endif /* HEAPTRACE_STACKTRACE_H */
