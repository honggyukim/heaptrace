/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTRACE_HEAPTRACE_H
#define HEAPTRACE_HEAPTRACE_H

#include <cstdio>

extern FILE *outfp;

#define TERM_COLOR_NORMAL ""
#define TERM_COLOR_RESET "\033[0m"
#define TERM_COLOR_BOLD "\033[1m"
#define TERM_COLOR_RED "\033[91m" /* bright red */
#define TERM_COLOR_GREEN "\033[32m"
#define TERM_COLOR_YELLOW "\033[33m"
#define TERM_COLOR_BLUE "\033[94m" /* bright blue */
#define TERM_COLOR_MAGENTA "\033[35m"
#define TERM_COLOR_CYAN "\033[36m"
#define TERM_COLOR_GRAY "\033[90m" /* bright black */

#ifdef DEBUG
#define pr_dbg(fmt, ...) fprintf(stderr, fmt, ##__VA_ARGS__)
#else
#define pr_dbg(fmt, ...)
#endif

#define pr_out(fmt, ...) fprintf(outfp, fmt, ##__VA_ARGS__)

#define pr_red(fmt, ...) fprintf(stdout, TERM_COLOR_RED fmt TERM_COLOR_RESET, ##__VA_ARGS__)

struct thread_flags_t {
	// to protect unexpected recursive malloc calls
	bool hook_guard;
};
extern thread_local struct thread_flags_t thread_flags;

struct opts {
	int idx;
	char *exename;

	int top;
	const char *sort_keys;
	bool flamegraph;
	char *outfile;
	char *ignore;
};

extern opts opts;

#endif /* HEAPTRACE_HEAPTRACE_H */
