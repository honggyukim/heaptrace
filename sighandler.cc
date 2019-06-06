/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <signal.h>

#include "heaptrace.h"
#include "stacktrace.h"

void sigusr1_handler(int signo)
{
	pr_dbg("\n=== sigusr1_handler(%d) ===\n", signo);
	dump_stackmap(ALLOC_SIZE, opts.flamegraph);
}

void sigusr2_handler(int signo)
{
	pr_dbg("\n=== sigusr2_handler(%d) ===\n", signo);
	dump_stackmap(ALLOC_COUNT, opts.flamegraph);
}
