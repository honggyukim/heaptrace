/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include "heaptrace.h"
#include "stacktrace.h"
#include "sighandler.h"

static void sigusr1_handler(int signo)
{
	pr_dbg("\n=== sigusr1_handler(%d) ===\n", signo);
	dump_stackmap("size", opts.flamegraph);
}

static void sigusr2_handler(int signo)
{
	pr_dbg("\n=== sigusr2_handler(%d) ===\n", signo);
	dump_stackmap("count", opts.flamegraph);
}

static void sigquit_handler(int signo)
{
	pr_dbg("\n=== sigquit_handler(%d) ===\n", signo);
	clear_stackmap();
}

void register_sighandler(sighandler_t handler, int signo)
{
	struct sigaction sig;

	sig.sa_handler = handler;
	sigemptyset(&sig.sa_mask);
	sig.sa_flags = 0;

	if (sigaction(signo, &sig, nullptr) == -1)
		pr_dbg("signal(%d) error", signo);
}

void sighandler_init(void)
{
	register_sighandler(&sigusr1_handler, SIGUSR1);
	register_sighandler(&sigusr2_handler, SIGUSR2);
	register_sighandler(&sigquit_handler, SIGQUIT);
}
