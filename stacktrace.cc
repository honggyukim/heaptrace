/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <dlfcn.h>

#include "heaptrace.h"
#include "stacktrace.h"

std::map<stack_trace_t, stack_info_t> stackmap;
std::map<addr_t, object_info_t> addrmap;

// record_backtrace() is defined in stacktrace.h as an inline function.
void __record_backtrace(size_t size, void* addr,
			       stack_trace_t& stack_trace, int nptrs)
{
	LOG("  record_backtrace(%zd, %p)\n", size, addr);

	struct stack_info_t& stack_info = stackmap[stack_trace];
	stack_info.total_size += size;
	stack_info.stack_depth = nptrs;
	stack_info.count++;

	struct object_info_t& object_info = addrmap[addr];
	object_info.stack_trace = stack_trace;
	object_info.size = size;
}

void release_backtrace(void* addr)
{
	LOG("  release_backtrace(%p)\n", addr);

	const auto& addrit = addrmap.find(addr);
	if (addrit == addrmap.end())
		return;

	object_info_t& object_info = addrit->second;
	stack_trace_t& stack_trace = object_info.stack_trace;

	const auto& stackit = stackmap.find(stack_trace);
	if (stackit == stackmap.end())
		return;

	stack_info_t& stack_info = stackit->second;
	stack_info.total_size -= object_info.size;
	stack_info.count--;
	if (stack_info.count == 0) {
		const auto& it = stackmap.find(stack_trace);
		if (it != stackmap.end())
			stackmap.erase(it);
	}
}

