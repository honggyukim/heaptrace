/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <stdlib.h>

int main(void)
{
	int *p;

	p = (int *)malloc(4);
	free(p);

	p = (int *)calloc(4, 10);

	p = (int *)realloc(p, 1000000);
	free(p);

	return 0;
}
