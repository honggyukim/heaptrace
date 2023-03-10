/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <argp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <sstream>
#include <string>

#include "heaptrace.h"

#define HEAPTRACE_VERSION "v0.01"

struct opts opts;

// output of --version option (generated by argp runtime)
const char *argp_program_version = "heaptrace " HEAPTRACE_VERSION;

// (a part of) output in --help option (generated by argp runtime)
const char *argp_program_bug_address = "http://mod.lge.com/hub/hong.gyu.kim/heaptrace/issues";

enum options {
	OPT_top = 301,
	OPT_sort,
	OPT_flamegraph,
	OPT_outfile,
};

static struct argp_option heaptrace_options[] = {
	{ "help", 'h', 0, 0, "Give this help list" },
	{ "top", OPT_top, "NUM", 0, "Set number of top backtraces to show (default 10)" },
	{ "sort", 's', "KEYs", 0, "Sort backtraces based on KEYs (size or count)" },
	{ "flame-graph", OPT_flamegraph, 0, 0, "Print heap trace info in flamegraph format" },
	{ "outfile", OPT_outfile, "FILE", 0, "Save log messages to this file" },
	{ 0 }
};

static error_t parse_option(int key, char *arg, struct argp_state *state)
{
	struct opts *opts = (struct opts *)state->input;

	switch (key) {
	case 'h':
		argp_state_help(state, state->out_stream, ARGP_HELP_STD_HELP);
		break;

	case OPT_top:
		opts->top = strtol(arg, NULL, 0);
		break;

	case 's':
		opts->sort_keys = arg;
		break;

	case OPT_flamegraph:
		opts->flamegraph = true;
		break;

	case OPT_outfile:
		opts->outfile = arg;
		break;

	case ARGP_KEY_ARG:
		if (state->arg_num)
			return ARGP_ERR_UNKNOWN;

		if (opts->exename == NULL) {
			// remaining options will be processed in ARGP_KEY_ARGS
			return ARGP_ERR_UNKNOWN;
		}

		break;

	case ARGP_KEY_ARGS:
		// process remaining non-option arguments
		opts->exename = state->argv[state->next];
		opts->idx = state->next;
		break;

	case ARGP_KEY_NO_ARGS:
	case ARGP_KEY_END:
		if (state->arg_num < 1)
			argp_usage(state);
		break;

	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static void init_options(int argc, char *argv[])
{
	struct argp argp = {
		heaptrace_options,
		parse_option,
		"[<program>]",
		"heaptrace -- collects and reports heap allocated memory",
	};

	// set default option values
	// TODO: create constexpr variables instead of default magic values.
	opts.top = 10;
	opts.sort_keys = "size";
	opts.flamegraph = false;

	argp_parse(&argp, argc, argv, ARGP_IN_ORDER, NULL, &opts);
}

static void setup_child_environ(struct opts *opts, int argc, char *argv[])
{
	std::stringstream ss;
	char buf[4096];
	char *old_preload = getenv("LD_PRELOAD");
	char *old_libpath = getenv("LD_LIBRARY_PATH");

	if (!access("libheaptrace.so", X_OK))
		ss << "./";
	ss << "libheaptrace.so";
	if (old_preload)
		ss << ":" << old_preload;

	// ensure heaptrace gets called at first
	setenv("LD_PRELOAD", ss.str().c_str(), 1);

	// ----- additional option processing -----

	snprintf(buf, sizeof(buf), "%d", opts->top);
	setenv("HEAPTRACE_NUM_TOP_BACKTRACE", buf, 1);

	setenv("HEAPTRACE_SORT_KEYS", opts->sort_keys, 1);

	snprintf(buf, sizeof(buf), "%d", opts->flamegraph);
	setenv("HEAPTRACE_FLAME_GRAPH", buf, 1);

	if (opts->outfile)
		setenv("HEAPTRACE_OUTFILE", opts->outfile, 1);
}

int main(int argc, char *argv[])
{
	init_options(argc, argv);

	// pass only non-heaptrace options to execv()
	argc -= opts.idx;
	argv += opts.idx;

	setup_child_environ(&opts, argc, argv);

	execv(opts.exename, argv);
	perror(opts.exename);

	return -1;
}
