/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <stdlib.h>

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-result"

__attribute__((noinline)) int fac(int n)
{
	malloc(n);
	if (n == 0) {
		calloc(10, 1);
		malloc(10);
		return 1;
	}
	else {
		return n * fac(n - 1);
	}
}

int main()
{
	free(malloc(1000));
	fac(7);
	return 0;
}

#pragma GCC diagnostic pop
