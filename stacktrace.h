/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTRACE_STACKTRACE_H
#define HEAPTRACE_STACKTRACE_H

#include "heaptrace.h"

void release_backtrace(void* addr)
{
	LOG("  release_backtrace(%p)\n", addr);
}

inline void record_backtrace(size_t size, void* addr)
{
	LOG("  record_backtrace(%zd, %p)\n", size, addr);
}

#endif /* HEAPTRACE_STACKTRACE_H */
