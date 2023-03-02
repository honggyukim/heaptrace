/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <inttypes.h>
#include <limits.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <cxxabi.h>
#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <vector>

#include <mutex>

#include "compiler.h"
#include "heaptrace.h"
#include "stacktrace.h"
#include "utils.h"

#define SYMBOL_MAXLEN 128

std::map<stack_trace_t, stack_info_t> stackmap;
std::map<addr_t, object_info_t> addrmap;

std::recursive_mutex container_mutex;

// record_backtrace() is defined in stacktrace.h as an inline function.
void __record_backtrace(size_t size, void *addr, stack_trace_t &stack_trace, int nptrs)
{
	std::lock_guard<std::recursive_mutex> lock(container_mutex);

	pr_dbg("  record_backtrace(%zd, %p)\n", size, addr);

	if (stackmap.find(stack_trace) == stackmap.end()) {
		// Record the creation time for the stack_trace
		struct stack_info_t stack_info {};
		stack_info.birth_time = std::chrono::steady_clock::now();
		stackmap[stack_trace] = stack_info;
	}

	struct stack_info_t &stack_info = stackmap[stack_trace];
	stack_info.stack_depth = nptrs;
	stack_info.total_size += size;
	stack_info.peak_total_size = std::max(stack_info.peak_total_size, stack_info.total_size);
	stack_info.count++;
	stack_info.peak_count = std::max(stack_info.peak_count, stack_info.count);

	struct object_info_t &object_info = addrmap[addr];
	object_info.stack_trace = stack_trace;
	object_info.size = size;
}

void release_backtrace(void *addr)
{
	if (unlikely(!addr))
		return;

	std::lock_guard<std::recursive_mutex> lock(container_mutex);

	pr_dbg("  release_backtrace(%p)\n", addr);

	const auto &addrit = addrmap.find(addr);
	if (unlikely(addrit == addrmap.end()))
		return;

	object_info_t &object_info = addrit->second;
	stack_trace_t &stack_trace = object_info.stack_trace;

	const auto &stackit = stackmap.find(stack_trace);
	if (unlikely(stackit == stackmap.end()))
		return;

	stack_info_t &stack_info = stackit->second;
	stack_info.total_size -= object_info.size;
	stack_info.count--;
	if (stack_info.count == 0) {
		// The stackmap for the given stacktrace is no longer needed.
		stackmap.erase(stackit);
	}

	// The given address is released so remove it from addrmap.
	addrmap.erase(addrit);
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
			symbol[len] = '\0';
		}
		offset = static_cast<int>(static_cast<int *>(addr) -
					  static_cast<int *>(dlip.dli_saddr));
		pr_out("%s +%#x ", symbol, offset);
		free(symbol);
	}
	offset = (int)((char *)addr - (char *)(dlip.dli_fbase));
	pr_out("(%s +%#x)\n", dlip.dli_fname, offset);
}

static void print_backtrace_symbol_flamegraph(void *addr, const char *semicolon)
{
	Dl_info dlip;
	char *symbol;
	int offset;
	int status;

	// dladdr() translates address to symbolic info.
	dladdr(addr, &dlip);

	symbol = abi::__cxa_demangle(dlip.dli_sname, 0, 0, &status);

	if (status == -2 && !symbol)
		symbol = strdup(dlip.dli_sname);

	if (symbol) {
		int len = SYMBOL_MAXLEN;

		if (strlen(symbol) > len) {
			symbol[len - 3] = '.';
			symbol[len - 2] = '.';
			symbol[len - 1] = '.';
			symbol[len] = '\0';
		}
		offset = static_cast<int>(static_cast<int *>(addr) -
					  static_cast<int *>(dlip.dli_saddr));
		pr_out("%s%s+%#x", semicolon, symbol, offset);
		free(symbol);
	}
	else {
		pr_out("%s%s+%p", semicolon, dlip.dli_fname, addr);
	}
}

static std::string get_delta_time_unit(std::chrono::nanoseconds delta)
{
	std::string str;

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
		str = utils::asprintf("%" PRId64 " hours %" PRId64 " mins", h.count(),
				      mins.count());
	else if (mins.count() > 0)
		str = utils::asprintf("%" PRId64 " mins %" PRId64 " secs", mins.count(),
				      secs.count());
	else if (secs.count() > 0)
		str = utils::asprintf("%" PRId64 ".%" PRId64 " secs", secs.count(), millis.count());
	else if (millis.count() > 0)
		str = utils::asprintf("%" PRId64 ".%" PRId64 " ms", millis.count(), micros.count());
	else if (micros.count() > 0)
		str = utils::asprintf("%" PRId64 ".%" PRId64 " us", micros.count(), nanos.count());
	else
		str = utils::asprintf("%" PRId64 " ns", nanos.count());

	return str;
}

static std::string get_byte_unit(uint64_t size)
{
	std::string str;
	int ret;

	utils::bytes sz(size);

	auto mb = std::chrono::duration_cast<utils::megabytes>(sz);
	sz -= mb;

	auto kb = std::chrono::duration_cast<utils::kilobytes>(sz);
	sz -= kb;

	auto b = sz;

	if (mb.count() > 0)
		str = utils::asprintf("%" PRId64 ".%" PRId64 " MB", mb.count(), kb.count());
	else if (kb.count() > 0)
		str = utils::asprintf("%" PRId64 ".%" PRId64 " KB", kb.count(), b.count());
	else
		str = utils::asprintf("%" PRId64 " bytes", b.count());

	return str;
}

std::string read_statm()
{
	int vss, rss, shared;
	int pagesize_kb = sysconf(_SC_PAGESIZE);
	std::ifstream fs("/proc/self/statm");

	fs >> vss >> rss >> shared;
	vss *= pagesize_kb;
	rss *= pagesize_kb;
	shared *= pagesize_kb;

	std::string str =
		get_byte_unit(vss) + " / " + get_byte_unit(rss) + " / " + get_byte_unit(shared);
	return str;
}

static void print_dump_stackmap_header(const char *sort_key)
{
	pr_out("[heaptrace] dump allocation sorted by '%s' for /proc/%d/maps (%s)\n", sort_key,
	       utils::gettid(), utils::get_comm_name().c_str());
}

static void
print_dump_stackmap_footer(const std::vector<std::pair<stack_trace_t, stack_info_t>> &sorted_stack)
{
	// get allocated size info from the allocator
	struct mallinfo minfo = mallinfo();

	uint64_t total_size = 0;
	size_t stack_size = sorted_stack.size();
	for (int i = 0; i < stack_size; i++) {
		const stack_info_t &sinfo = sorted_stack[i].second;
		total_size += sinfo.total_size;
	}

	pr_out("[heaptrace] heap traced num of backtrace : %zd\n", stack_size);

	pr_out("[heaptrace] heap traced allocation size  : %s\n",
	       get_byte_unit(total_size).c_str());

	pr_out("[heaptrace] allocator info (virtual)     : %s\n",
	       get_byte_unit(minfo.arena + minfo.hblkhd).c_str());
	pr_out("[heaptrace] allocator info (resident)    : %s\n",
	       get_byte_unit(minfo.uordblks).c_str());

	pr_out("[heaptrace] statm info (VSS/RSS/shared)  : %s\n", read_statm().c_str());
}

static void print_dump_stackmap(std::vector<std::pair<stack_trace_t, stack_info_t>> &sorted_stack)
{
	const time_point_t current = std::chrono::steady_clock::now();
	int cnt = 0;

	size_t stack_size = sorted_stack.size();
	for (int i = 0; i < stack_size; i++) {
		const stack_info_t &info = sorted_stack[i].second;

		if (i >= opts.top)
			break;

		const stack_trace_t &stack_trace = sorted_stack[i].first;
		std::string age = get_delta_time_unit(current - info.birth_time);

		pr_out("=== backtrace #%d === [count/peak: %zd/%zd] "
		       "[size/peak: %s/%s] [age: %s]\n",
		       ++cnt, info.count, info.peak_count, get_byte_unit(info.total_size).c_str(),
		       get_byte_unit(info.peak_total_size).c_str(), age.c_str());

		for (int j = 0; j < info.stack_depth; j++)
			print_backtrace_symbol(j, stack_trace[j]);

		pr_out("\n");
	}
}

static void
print_dump_stackmap_flamegraph(std::vector<std::pair<stack_trace_t, stack_info_t>> &sorted_stack)
{
	size_t stack_size = sorted_stack.size();
	for (int i = 0; i < stack_size; i++) {
		const stack_info_t &info = sorted_stack[i].second;
		size_t count = info.count;
		uint64_t size = info.total_size;
		const char *semicolon = "";

		if (i >= opts.top)
			break;

		const stack_trace_t &stack_trace = sorted_stack[i].first;

		for (int j = info.stack_depth - 1; j >= 0; j--) {
			print_backtrace_symbol_flamegraph(stack_trace[j], semicolon);
			semicolon = ";";
		}
		pr_out(" %" PRIu64 "\n", size);
	}

	fflush(outfp);
}

static void sort_stack(const std::string &order,
		       std::vector<std::pair<stack_trace_t, stack_info_t>> &sorted_stack)
{
	std::sort(sorted_stack.begin(), sorted_stack.end(),
		  [&order](const std::pair<stack_trace_t, stack_info_t> &p1,
			   const std::pair<stack_trace_t, stack_info_t> &p2) {
			  if (order == "count") {
				  if (p1.second.count == p2.second.count)
					  return p1.second.total_size > p2.second.total_size;
				  return p1.second.count > p2.second.count;
			  }
			  else {
				  // sort based on size for unknown sort order.
				  if (p1.second.total_size == p2.second.total_size)
					  return p1.second.count > p2.second.count;
				  return p1.second.total_size > p2.second.total_size;
			  }
		  });
}

void dump_stackmap(const char *sort_keys, bool flamegraph)
{
	auto *tfs = &thread_flags;

	if (stackmap.empty())
		return;

	tfs->hook_guard = true;

	std::vector<std::string> sort_key_vec = utils::string_split(sort_keys, ',');

	// sort the stack trace based on the count and then total_size
	std::vector<std::pair<stack_trace_t, stack_info_t>> sorted_stack;
	{
		// protect stackmap access
		std::lock_guard<std::recursive_mutex> lock(container_mutex);

		for (auto &p : stackmap)
			sorted_stack.push_back(make_pair(p.first, p.second));
	}

	if (flamegraph) {
		// use only the first sort order given by -s/--sort option.
		sort_stack(sort_key_vec.front(), sorted_stack);
		print_dump_stackmap_flamegraph(sorted_stack);
	}
	else {
		pr_out("=================================================================\n");
		for (const auto &sort_key : sort_key_vec) {
			print_dump_stackmap_header(sort_key.c_str());
			sort_stack(sort_key, sorted_stack);
			print_dump_stackmap(sorted_stack);
		}
		print_dump_stackmap_footer(sorted_stack);
		pr_out("=================================================================\n");
		fflush(outfp);
	}

	tfs->hook_guard = false;
}

void clear_stackmap(void)
{
	std::lock_guard<std::recursive_mutex> lock(container_mutex);
	auto *tfs = &thread_flags;

	tfs->hook_guard = true;

	stackmap.clear();
	addrmap.clear();

	tfs->hook_guard = false;
}
