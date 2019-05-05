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
}

extern "C"
void* malloc(size_t size)
{
	if (unlikely(!real_malloc))
		return __libc_malloc(size);

	void* p = real_malloc(size);
	LOG("malloc(%zd) = %p\n", size, p);

	return p;
}

extern "C"
void free(void *ptr)
{
	if (unlikely(!real_free)) {
		__libc_free(ptr);
		return;
	}

	LOG("free(%p)\n", ptr);
	real_free(ptr);
}

extern "C"
void *calloc(size_t nmemb, size_t size)
{
	if (unlikely(!real_calloc))
		return __libc_calloc(nmemb, size);

	void* p = real_calloc(nmemb, size);
	LOG("calloc(%zd, %zd) = %p\n", nmemb, size, p);

	return p;
}

extern "C"
void *realloc(void *ptr, size_t size)
{
	if (unlikely(!real_realloc))
		return __libc_realloc(ptr, size);

	void* p = real_realloc(ptr, size);
	LOG("realloc(%p, %zd) = %p\n", ptr, size, p);

	return p;
}
