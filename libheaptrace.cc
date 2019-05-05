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

#include "compiler.h"

#define LOG(fmt, ...) fprintf(stderr, fmt, ## __VA_ARGS__)

// dlsym internally uses calloc, so use weak symbol to get their symbol
extern "C" __weak void* __libc_malloc(size_t size);
extern "C" __weak void  __libc_free(void* ptr);
extern "C" __weak void* __libc_calloc(size_t nmemb, size_t size);
extern "C" __weak void* __libc_realloc(void *ptr, size_t size);

typedef void* (*MallocFunction)(size_t size);
typedef void  (*FreeFunction)(void *ptr);
typedef void* (*CallocFunction)(size_t nmemb, size_t size);
typedef void* (*ReallocFunction)(void *ptr, size_t size);

static MallocFunction  real_malloc;
static FreeFunction    real_free;
static CallocFunction  real_calloc;
static ReallocFunction real_realloc;

// This flag is needed because printf internally calls malloc again.
static bool hook_guard;

__constructor
static void heaptrace_init()
{
	if (!real_malloc)
		real_malloc = (MallocFunction)dlsym(RTLD_NEXT, "malloc");
	if (!real_free)
		real_free = (FreeFunction)dlsym(RTLD_NEXT, "free");
	if (!real_calloc)
		real_calloc = (CallocFunction)dlsym(RTLD_NEXT, "calloc");
	if (!real_realloc)
		real_realloc = (ReallocFunction)dlsym(RTLD_NEXT, "realloc");
	LOG("=== heaptrace init ===\n");
}

__destructor
static void heaptrace_fini()
{
	LOG("=== heaptrace fini ===\n");

	// disable any other hooking after this.
	hook_guard = true;
}

extern "C"
void* malloc(size_t size)
{
	if (unlikely(hook_guard || !real_malloc))
		return __libc_malloc(size);

	hook_guard = true;

	void* p = real_malloc(size);
	LOG("malloc(%zd) = %p\n", size, p);

	hook_guard = false;

	return p;
}

extern "C"
void free(void *ptr)
{
	if (unlikely(hook_guard || !real_free)) {
		__libc_free(ptr);
		return;
	}

	hook_guard = true;

	LOG("free(%p)\n", ptr);
	real_free(ptr);

	hook_guard = false;
}

extern "C"
void *calloc(size_t nmemb, size_t size)
{
	if (unlikely(hook_guard || !real_calloc))
		return __libc_calloc(nmemb, size);

	hook_guard = true;

	void* p = real_calloc(nmemb, size);
	LOG("calloc(%zd, %zd) = %p\n", nmemb, size, p);

	hook_guard = false;

	return p;
}

extern "C"
void *realloc(void *ptr, size_t size)
{
	if (unlikely(hook_guard || !real_realloc))
		return __libc_realloc(ptr, size);

	hook_guard = true;

	void* p = real_realloc(ptr, size);
	LOG("realloc(%p, %zd) = %p\n", ptr, size, p);

	hook_guard = false;

	return p;
}
