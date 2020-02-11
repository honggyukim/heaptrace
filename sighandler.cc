/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <signal.h>

#include "heaptrace.h"
#include "stacktrace.h"

static void sigusr1_handler(int signo)
{
	pr_dbg("\n=== sigusr1_handler(%d) ===\n", signo);
	dump_stackmap(ALLOC_SIZE, opts.flamegraph);
}

static void sigusr2_handler(int signo)
{
	pr_dbg("\n=== sigusr2_handler(%d) ===\n", signo);
	dump_stackmap(ALLOC_COUNT, opts.flamegraph);
}

static void sigquit_handler(int signo)
{
	pr_dbg("\n=== sigquit_handler(%d) ===\n", signo);
	clear_stackmap();
}

void sighandler_init(void)
{
	struct sigaction sigusr1, sigusr2, sigquit;

	sigusr1.sa_handler = sigusr1_handler;
	sigemptyset(&sigusr1.sa_mask);
	sigusr1.sa_flags = 0;

	sigusr2.sa_handler = sigusr2_handler;
	sigemptyset(&sigusr2.sa_mask);
	sigusr2.sa_flags = 0;

	sigquit.sa_handler = sigquit_handler;
	sigemptyset(&sigquit.sa_mask);
	sigquit.sa_flags = 0;

	if (sigaction(SIGUSR1, &sigusr1, 0) == -1)
		pr_dbg("signal(SIGUSR1) error");

	if (sigaction(SIGUSR2, &sigusr2, 0) == -1)
		pr_dbg("signal(SIGUSR2) error");

	if (sigaction(SIGQUIT, &sigquit, 0) == -1)
		pr_dbg("signal(SIGQUIT) error");
}
