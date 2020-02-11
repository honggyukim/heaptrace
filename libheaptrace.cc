/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <unistd.h>
#include <dlfcn.h>
#include <signal.h>
#include <sys/mman.h>

#include <sstream>

#include "heaptrace.h"
#include "compiler.h"
#include "stacktrace.h"
#include "sighandler.h"
#include "utils.h"

// dlsym internally uses calloc, so use weak symbol to get their symbol
extern "C" __weak void* __libc_malloc(size_t size);
extern "C" __weak void  __libc_free(void* ptr);
extern "C" __weak void* __libc_calloc(size_t nmemb, size_t size);
extern "C" __weak void* __libc_realloc(void *ptr, size_t size);
extern "C" __weak void* __libc_memalign(size_t alignment, size_t size);
extern "C" __weak int   __posix_memalign(void **memptr, size_t alignment, size_t size);

typedef void* (*MallocFunction)(size_t size);
typedef void  (*FreeFunction)(void *ptr);
typedef void* (*CallocFunction)(size_t nmemb, size_t size);
typedef void* (*ReallocFunction)(void *ptr, size_t size);
typedef void* (*MemalignFunction)(size_t alignment, size_t size);
typedef int   (*PosixMemalignFunction)(void **memptr, size_t alignment, size_t size);
typedef void* (*MmapFunction)(void *addr, size_t length, int prot, int flags, int fd, off_t offset);
typedef int   (*MunmapFunction)(void *addr, size_t length);

static MallocFunction        real_malloc;
static FreeFunction          real_free;
static CallocFunction        real_calloc;
static ReallocFunction       real_realloc;
static MemalignFunction      real_memalign;
static PosixMemalignFunction real_posix_memalign;

thread_local struct thread_flags_t thread_flags;

// true only when heaptrace_init is done
static bool initialized;

struct opts opts;

FILE *outfp;

__constructor
static void heaptrace_init()
{
	auto* tfs = &thread_flags;
	int pid = getpid();
	std::stringstream ss;
	std::string comm = utils::get_comm_name();

	real_malloc = (MallocFunction)dlsym(RTLD_NEXT, "malloc");
	real_free = (FreeFunction)dlsym(RTLD_NEXT, "free");
	real_calloc = (CallocFunction)dlsym(RTLD_NEXT, "calloc");
	real_realloc = (ReallocFunction)dlsym(RTLD_NEXT, "realloc");
	real_memalign = (MemalignFunction)dlsym(RTLD_NEXT, "memalign");
	real_posix_memalign = (PosixMemalignFunction)dlsym(RTLD_NEXT, "posix_memalign");

	// initialize signal handlers
	sighandler_init();

	// setup option values
	opts.top = strtol(getenv("HEAPTRACE_NUM_TOP_BACKTRACE"), NULL, 0);
	opts.sortkey = getenv("HEAPTRACE_SORTKEY");
	opts.flamegraph = strtol(getenv("HEAPTRACE_FLAME_GRAPH"), NULL, 0);

	opts.outfile = getenv("HEAPTRACE_OUTFILE");
	if (opts.outfile) {
		ss << opts.outfile << "." << pid
				   << "." << comm.c_str();
		outfp = fopen(ss.str().c_str(), "w");
	}
	else
		outfp = stdout;

	if (!opts.flamegraph) {
		pr_out("[heaptrace] initialized for /proc/%d/maps (%s)\n",
			pid, comm.c_str());
	}

	initialized = true;
}

__destructor
static void heaptrace_fini()
{
	auto* tfs = &thread_flags;
	int pid = getpid();
	enum alloc_sort_order order = ALLOC_SIZE;
	std::string comm = utils::get_comm_name();

	if (!opts.flamegraph) {
		pr_out("[heaptrace]   finalized for /proc/%d/maps (%s)\n",
			pid, comm.c_str());
	}

	if (!strcmp(opts.sortkey, "count"))
		order = ALLOC_COUNT;

	dump_stackmap(order, opts.flamegraph);

	if (opts.outfile)
		fclose(outfp);

	// disable any other hooking after this.
	tfs->hook_guard = true;
}

__visible_default
void* operator new(size_t size)
{
	auto* tfs = &thread_flags;

	if (unlikely(tfs->hook_guard || !initialized))
		return __libc_malloc(size);

	tfs->hook_guard = true;

	void* p = real_malloc(size);
	pr_dbg("operator new(%zd) = %p\n", size, p);
	record_backtrace(size, p);

	tfs->hook_guard = false;

	return p;
}

__visible_default
void* operator new[](size_t size)
{
	auto* tfs = &thread_flags;

	if (unlikely(tfs->hook_guard || !initialized))
		return __libc_malloc(size);

	tfs->hook_guard = true;

	void* p = real_malloc(size);
	pr_dbg("operator new[](%zd) = %p\n", size, p);
	record_backtrace(size, p);

	tfs->hook_guard = false;

	return p;
}

__visible_default
void operator delete(void *ptr)
{
	auto* tfs = &thread_flags;

	if (unlikely(tfs->hook_guard || !initialized)) {
		__libc_free(ptr);
		return;
	}

	tfs->hook_guard = true;

	pr_dbg("operator delete(%p)\n", ptr);
	release_backtrace(ptr);
	real_free(ptr);

	tfs->hook_guard = false;
}

__visible_default
void operator delete[](void *ptr)
{
	auto* tfs = &thread_flags;

	if (unlikely(tfs->hook_guard || !initialized)) {
		__libc_free(ptr);
		return;
	}

	tfs->hook_guard = true;

	pr_dbg("operator delete[](%p)\n", ptr);
	release_backtrace(ptr);
	real_free(ptr);

	tfs->hook_guard = false;
}

extern "C" __visible_default
void* malloc(size_t size)
{
	auto* tfs = &thread_flags;

	if (unlikely(tfs->hook_guard || !initialized))
		return __libc_malloc(size);

	tfs->hook_guard = true;

	void* p = real_malloc(size);
	pr_dbg("malloc(%zd) = %p\n", size, p);
	record_backtrace(size, p);

	tfs->hook_guard = false;

	return p;
}

extern "C" __visible_default
void free(void *ptr)
{
	auto* tfs = &thread_flags;

	if (unlikely(tfs->hook_guard || !initialized)) {
		__libc_free(ptr);
		return;
	}

	tfs->hook_guard = true;

	pr_dbg("free(%p)\n", ptr);
	release_backtrace(ptr);
	real_free(ptr);

	tfs->hook_guard = false;
}

extern "C" __visible_default
void *calloc(size_t nmemb, size_t size)
{
	auto* tfs = &thread_flags;

	if (unlikely(tfs->hook_guard || !initialized))
		return __libc_calloc(nmemb, size);

	tfs->hook_guard = true;

	void* p = real_calloc(nmemb, size);
	pr_dbg("calloc(%zd, %zd) = %p\n", nmemb, size, p);
	record_backtrace(nmemb * size, p);

	tfs->hook_guard = false;

	return p;
}

extern "C" __visible_default
void *realloc(void *ptr, size_t size)
{
	auto* tfs = &thread_flags;

	if (unlikely(tfs->hook_guard || !initialized))
		return __libc_realloc(ptr, size);

	tfs->hook_guard = true;

	void* p = real_realloc(ptr, size);
	pr_dbg("realloc(%p, %zd) = %p\n", ptr, size, p);
	release_backtrace(ptr);
	record_backtrace(size, p);

	tfs->hook_guard = false;

	return p;
}

extern "C" __visible_default
void *memalign(size_t alignment, size_t size)
{
	auto *tfs = &thread_flags;

	if (unlikely(tfs->hook_guard || !initialized))
		return __libc_memalign(alignment, size);

	tfs->hook_guard = true;

	void *p = real_memalign(alignment, size);
	pr_dbg("memalign(%zd, %zd) = %p\n", alignment, size, p);
	record_backtrace(size, p);

	tfs->hook_guard = false;

	return p;
}

extern "C" __visible_default
int posix_memalign(void **memptr, size_t alignment, size_t size)
{
	auto *tfs = &thread_flags;

	if (unlikely(!real_posix_memalign))
		real_posix_memalign = (PosixMemalignFunction)dlsym(RTLD_NEXT, "posix_memalign");

	if (unlikely(tfs->hook_guard || !initialized))
		return real_posix_memalign(memptr, alignment, size);

	tfs->hook_guard = true;

	int ret = real_posix_memalign(memptr, alignment, size);
	pr_dbg("posix_memalign(%p, %zd, %zd) = %d\n", memptr, alignment, size, ret);
	if (ret == 0)
		record_backtrace(size, *memptr);

	tfs->hook_guard = false;

	return ret;
}

