/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <cinttypes>
#include <climits>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <malloc.h>

#include <cxxabi.h>
#include <dlfcn.h>
#include <unistd.h>

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>
#include <vector>
#include <mutex>

#include "compiler.h"
#include "heaptrace.h"
#include "stacktrace.h"
#include "utils.h"

#define SYMBOL_MAXLEN 128

#if (__GLIBC__ > 2) || (__GLIBC__ == 2 && __GLIBC_MINOR__ >= 33)
#define GLIBC_233_OR_LATER
#endif

std::map<stack_trace_t, stack_info_t> stackmap;
std::map<addr_t, object_info_t> addrmap;
std::vector<std::string> ignorevec;
bool ignorevec_initialized = false;

std::recursive_mutex container_mutex;

static void lazyinit_ignorevec()
{
	if (ignorevec_initialized)
		return;

	opts.ignore = getenv("HEAPTRACE_IGNORE");
	if (opts.ignore) {
		std::ifstream file(opts.ignore);
		if (file.is_open()) {
			std::string line;
			while (std::getline(file, line)) {
				ignorevec.push_back(line);
			}
			file.close();
		}
		else {
			pr_out("Failed to open file %s\n", opts.ignore);
		}
	}
	ignorevec_initialized = true;
}

static bool is_ignored(const std::string &report)
{
	lazyinit_ignorevec();
	return std::any_of(ignorevec.begin(), ignorevec.end(), [&report](const std::string& s)
			   { return report.find(s) != std::string::npos; });
}

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

static void get_backtrace_string(int count, void *addr, std::stringstream &ss_bt)
{
	Dl_info dlip;
	char *symbol;
	int offset;
	int status;
	int dl_ret;
	int len = SYMBOL_MAXLEN;

	ss_bt << std::dec << count << " [0x" << std::hex <<
		std::setw(4 + __SIZEOF_LONG__) << (unsigned long)addr << "] ";
	// dladdr() translates address to symbolic info.
	dl_ret = dladdr(addr, &dlip);
	if (dl_ret == 0) {
		ss_bt << "?\n";
		return;
	}

	if (dlip.dli_sname != nullptr && dlip.dli_saddr != nullptr) {
		symbol = abi::__cxa_demangle(dlip.dli_sname, nullptr, nullptr, &status);
		if (status != 0)
			symbol = strdup(dlip.dli_sname);

		if (strlen(symbol) > len) {
			symbol[len - 3] = '.';
			symbol[len - 2] = '.';
			symbol[len - 1] = '.';
			symbol[len] = '\0';
		}
		offset = static_cast<int>(static_cast<int *>(addr) -
					  static_cast<int *>(dlip.dli_saddr));
		ss_bt << symbol << " +0x" << offset << " ";
		free(symbol);
	}
	offset = (int)((char *)addr - (char *)(dlip.dli_fbase));
	ss_bt << "(" << dlip.dli_fname << " +0x" << offset << ")\n";
}

static void get_backtrace_string_flamegraph(void *addr, const char *semicolon,
					    std::stringstream &ss_bt)
{
	Dl_info dlip;
	char *symbol;
	int offset;
	int status;

	// dladdr() translates address to symbolic info.
	dladdr(addr, &dlip);

	symbol = abi::__cxa_demangle(dlip.dli_sname, nullptr, nullptr, &status);

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
		ss_bt << semicolon << symbol << "+0x" << offset;
		free(symbol);
	}
	else {
		ss_bt << semicolon << dlip.dli_fname << addr;
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
	long vss;
	long rss;
	long shared;
	long pagesize_kb = sysconf(_SC_PAGESIZE);
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
	pr_out("[heaptrace] dump allocation sorted by '%s' for /proc/%ld/maps (%s)\n", sort_key,
	       utils::gettid(), utils::get_comm_name().c_str());
}

static void
print_dump_stackmap_footer(const std::vector<std::pair<stack_trace_t, stack_info_t>> &sorted_stack)
{
	/*
	 * Get allocated size information from the allocator. mallinfo() is
	 * deprecated because it uses int for variables, which can only support
	 * memory sizes less than 4GB. Therefore, use mallinfo() only in
	 * environments with old glibc versions less than 2.33
	 */
#ifdef GLIBC_233_OR_LATER
	struct mallinfo2 minfo = mallinfo2();
#else
	struct mallinfo minfo = mallinfo();
#endif

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
	int cnt = 1;
	int top = opts.top;
	int i = 0;

	size_t stack_size = sorted_stack.size();
	while (i < stack_size && i < top) {
		const stack_info_t &info = sorted_stack[i].second;
		const stack_trace_t &stack_trace = sorted_stack[i].first;
		std::string age = get_delta_time_unit(current - info.birth_time);
		std::stringstream ss_intro;
		std::stringstream ss_bt;

		ss_intro << "=== backtrace #" << cnt << " === [count/peak: " << info.count << "/"
			 << info.peak_count << "] "
			 << "[size/peak: " << get_byte_unit(info.total_size) << "/"
			 << get_byte_unit(info.peak_total_size) << "] [age: " << age << "]\n";
		ss_bt << std::setfill('0');
		for (int j = 0; j < info.stack_depth; j++)
			get_backtrace_string(j, stack_trace[j], ss_bt);

		if (is_ignored(ss_bt.str())) {
			++top;
		}
		else {
			pr_out("%s%s\n", ss_intro.str().c_str(), ss_bt.str().c_str());
			++cnt;
		}
		++i;
	}
}

static void
print_dump_stackmap_flamegraph(std::vector<std::pair<stack_trace_t, stack_info_t>> &sorted_stack)
{
	size_t stack_size = sorted_stack.size();
	int i = 0;
	int top = opts.top;

	while (i < stack_size && i < top) {
		const stack_info_t &info = sorted_stack[i].second;
		uint64_t size = info.total_size;
		const char *semicolon = "";
		const stack_trace_t &stack_trace = sorted_stack[i].first;
		std::stringstream ss_bt;

		ss_bt << std::hex;
		for (size_t j = 0; j < info.stack_depth; ++j) {
			get_backtrace_string_flamegraph(stack_trace[info.stack_depth - 1 - j],
							semicolon, ss_bt);
			semicolon = ";";
		}
		if (is_ignored(ss_bt.str())) {
			++top;
		}
		else {
			pr_out("%s", ss_bt.str().c_str());
			pr_out(" %" PRIu64 "\n", size);
		}
		++i;
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
			sorted_stack.emplace_back(p.first, p.second);
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
