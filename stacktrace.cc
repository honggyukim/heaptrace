/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <limits.h>
#include <inttypes.h>
#include <sys/syscall.h>

#include <unistd.h>
#include <dlfcn.h>
#include <cxxabi.h>

#include <algorithm>
#include <vector>
#include <map>

#include <mutex>

#include "heaptrace.h"
#include "stacktrace.h"
#include "utils.h"

#define SYMBOL_MAXLEN 128

std::map<stack_trace_t, stack_info_t> stackmap;
std::map<addr_t, object_info_t> addrmap;

std::mutex container_mutex;

// record_backtrace() is defined in stacktrace.h as an inline function.
void __record_backtrace(size_t size, void* addr,
			       stack_trace_t& stack_trace, int nptrs)
{
	std::lock_guard<std::mutex> lock(container_mutex);

	pr_dbg("  record_backtrace(%zd, %p)\n", size, addr);

	if (stackmap.find(stack_trace) == stackmap.end()) {
		// Record the creation time for the stack_trace
		struct stack_info_t stack_info = { 0 };
		stack_info.birth_time = std::chrono::steady_clock::now();
		stackmap[stack_trace] = stack_info;
	}

	struct stack_info_t& stack_info = stackmap[stack_trace];
	stack_info.stack_depth = nptrs;
	stack_info.total_size += size;
	stack_info.max_total_size = std::max(stack_info.max_total_size,
					     stack_info.total_size);
	stack_info.count++;
	stack_info.max_count = std::max(stack_info.max_count, stack_info.count);

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

static void print_backtrace_symbol(int count, void *addr)
{
	Dl_info dlip;
	char *symbol;
	int offset;
	int status;

	// dladdr() translates address to symbolic info.
	dladdr(addr, &dlip);

#if __SIZEOF_LONG__ == 4
	pr_out("%2d [%#10lx] ", count, (unsigned long)addr);
#else
	pr_out("%2d [%#14lx] ", count, (unsigned long)addr);
#endif

	symbol = abi::__cxa_demangle(dlip.dli_sname, 0, 0, &status);

	if (status == -2 && !symbol)
		symbol = strdup(dlip.dli_sname);

	if (symbol) {
		int len = SYMBOL_MAXLEN;

		if (strlen(symbol) > len) {
			symbol[len - 3] = '.';
			symbol[len - 2] = '.';
			symbol[len - 1] = '.';
			symbol[len]     = '\0';
		}
		offset = static_cast<int>(static_cast<int*>(addr) -
					  static_cast<int*>(dlip.dli_saddr));
		pr_out("%s +%#x\n", symbol, offset);
		free(symbol);
	}
	else {
		pr_out("%s (+%p)\n", dlip.dli_fname, addr);
	}
}

static fmt_string get_delta_time_unit(std::chrono::nanoseconds delta)
{
	fmt_string str;
	int ret;

	auto h = std::chrono::duration_cast<std::chrono::hours>(delta);
	delta -= h;

	auto mins = std::chrono::duration_cast<std::chrono::minutes>(delta);
	delta -= mins;

	auto secs = std::chrono::duration_cast<std::chrono::seconds>(delta);
	delta -= secs;

	auto millis = std::chrono::duration_cast<std::chrono::milliseconds>(delta);
	delta -= millis;

	auto micros = std::chrono::duration_cast<std::chrono::microseconds>(delta);
	delta -= micros;

	auto nanos = delta;

	if (h.count() > 0)
		str.fmtset("%" PRId64 " hours %" PRId64 " mins", h.count(), mins.count());
	else if (mins.count() > 0)
		str.fmtset("%" PRId64 " mins %" PRId64 " secs", mins.count(), secs.count());
	else if (secs.count() > 0)
		str.fmtset("%" PRId64 ".%" PRId64 " secs", secs.count(), millis.count());
	else if (millis.count() > 0)
		str.fmtset("%" PRId64 ".%" PRId64 " ms", millis.count(), micros.count());
	else if (micros.count() > 0)
		str.fmtset("%" PRId64 ".%" PRId64 " us", micros.count(), nanos.count());
	else
		str.fmtset("%" PRId64 " ns", nanos.count());

	return str;
}

static void print_dump_header(void)
{
	int ret;
	int tid = syscall(SYS_gettid);
	fmt_string file_comm("/proc/%d/comm", tid);
	fmt_string comm(32);

	file_t file(file_comm);
	if (file)
		ret = fscanf(file, "%s", comm.data());

	pr_out("\n=================================================================\n");
	pr_out("    heaptrace of tid %d (%s)\n", tid, comm.get());
	pr_out("=================================================================\n");
}

void dump_stackmap(enum alloc_sort_order order)
{
	auto* tfs = &thread_flags;
	int alloc_size;
	uint64_t total_size = 0;
	int cnt = 0;
	time_point_t current;

	if (stackmap.empty()) {
		pr_out("\n");
		return;
	}

	tfs->hook_guard = true;

	// get allocated size info from the allocator
	struct mallinfo info = mallinfo();
	alloc_size = info.uordblks;

	// get current time
	current = std::chrono::steady_clock::now();

	// sort the stack trace based on the count and then total_size
	std::vector<std::pair<stack_trace_t, stack_info_t>> sorted_stack;
	{
		// protect stackmap access
		std::lock_guard<std::mutex> lock(container_mutex);

		for (auto& p : stackmap)
			sorted_stack.push_back(make_pair(p.first, p.second));
	}
	std::sort(sorted_stack.begin(), sorted_stack.end(),
		[order](std::pair<stack_trace_t, stack_info_t>& p1,
		   std::pair<stack_trace_t, stack_info_t>& p2) {
			if (order == ALLOC_COUNT) {
				if (p1.second.count == p2.second.count)
					return p1.second.total_size > p2.second.total_size;
				return p1.second.count > p2.second.count;
			}
			else if (order == ALLOC_SIZE) {
				if (p1.second.total_size == p2.second.total_size)
					return p1.second.count > p2.second.count;
				return p1.second.total_size > p2.second.total_size;
			}
	});

	print_dump_header();

	size_t stack_size = sorted_stack.size();
	for (int i = 0; i < stack_size; i++) {
		const stack_info_t& info = sorted_stack[i].second;

		total_size += info.total_size;

		if (i >= opts.top)
			continue;

		const stack_trace_t& stack_trace = sorted_stack[i].first;
		fmt_string age = get_delta_time_unit(current - info.birth_time);

		pr_out("=== stackmap #%d === [count/max: %zd/%zd] "
		       "[size/max: %" PRIu64 "/%" PRIu64 "] [age: %s]\n",
			++cnt, info.count, info.max_count,
			info.total_size, info.max_total_size, age.get());

		for (int i = 0; i < info.stack_depth; i++)
			print_backtrace_symbol(i, stack_trace[i]);

		pr_out("\n");
	}

	pr_out("Total size allocated %" PRIu64 "(%d) in top %d out of %zd stack trace\n\n",
		total_size, alloc_size, cnt, stack_size);

	tfs->hook_guard = false;
}
