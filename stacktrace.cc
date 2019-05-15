/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>

#include <unistd.h>
#include <dlfcn.h>

#include <algorithm>
#include <vector>
#include <map>

#include <mutex>

#include "heaptrace.h"
#include "stacktrace.h"

std::map<stack_trace_t, stack_info_t> stackmap;
std::map<addr_t, object_info_t> addrmap;

std::mutex container_mutex;

// record_backtrace() is defined in stacktrace.h as an inline function.
void __record_backtrace(size_t size, void* addr,
			       stack_trace_t& stack_trace, int nptrs)
{
	std::lock_guard<std::mutex> lock(container_mutex);

	pr_dbg("  record_backtrace(%zd, %p)\n", size, addr);

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
	std::lock_guard<std::mutex> lock(container_mutex);

	pr_dbg("  release_backtrace(%p)\n", addr);

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

void dump_stackmap(void)
{
	int alloc_size;
	size_t total_size = 0;
	int cnt = 0;
	char **strings;

	hook_guard = true;

	// get allocated size info from the allocator
	struct mallinfo info = mallinfo();
	alloc_size = info.uordblks;

	// sort the stack trace based on the count and then total_size
	std::vector<std::pair<stack_trace_t, stack_info_t>> sorted_stack;
	{
		// protect stackmap access
		std::lock_guard<std::mutex> lock(container_mutex);

		for (auto& p : stackmap)
			sorted_stack.push_back(make_pair(p.first, p.second));
	}
	std::sort(sorted_stack.begin(), sorted_stack.end(),
		[](std::pair<stack_trace_t, stack_info_t>& p1,
		   std::pair<stack_trace_t, stack_info_t>& p2) {
			if (p1.second.count == p2.second.count)
				return p1.second.total_size > p2.second.total_size;
			return p1.second.count > p2.second.count;
	});

	size_t stack_size = sorted_stack.size();
	for (int i = 0; i < stack_size; i++) {
		const stack_info_t& info = sorted_stack[i].second;

		total_size += info.total_size;

		// print top 10 stack trace
		if (i >= NUM_TOP_BACKTRACE)
			continue;

		const stack_trace_t& stack_trace = sorted_stack[i].first;

		pr_out("stackmap %d allocated %zd bytes (%zd times) ===\n",
		    cnt++, info.total_size, info.count);

		// search symbols of backtrace info
		strings = backtrace_symbols(stack_trace.data(), info.stack_depth);
		if (strings == NULL) {
			perror("backtrace_symbols");
			exit(EXIT_FAILURE);
		}
		for (int i = 0; i < info.stack_depth; i++)
			pr_out("%p: %s\n", stack_trace[i], strings[i]);
		pr_out("\n");
	}

	pr_out("Total size allocated %zd(%d) in top %d out of %zd stack trace\n",
		total_size, alloc_size, cnt, stack_size);

	hook_guard = false;
}
