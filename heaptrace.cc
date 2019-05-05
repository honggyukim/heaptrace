/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char *argv[])
{
	char buf[4096];
	char *old_preload = getenv("LD_PRELOAD");
	char *old_libpath = getenv("LD_LIBRARY_PATH");

	// ensure heaptrace gets called at first
	snprintf(buf, sizeof(buf), "libheaptrace.so:%s", old_preload ?: "");
	setenv("LD_PRELOAD", buf, 1);

	execv(argv[1], &argv[1]);

	return 0;
}
