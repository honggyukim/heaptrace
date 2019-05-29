/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTRACE_UTILS_H
#define HEAPTRACE_UTILS_H

#include <cstdio>

class file_t {
public:
	explicit file_t(const char* filename, const char* mode = "r") {
		fp = fopen(filename, mode);
	}
	~file_t() {
		if (fp) fclose(fp);
	}
	operator FILE*()         { return fp; }
	explicit operator bool() { return fp; }

private:
	FILE *fp;
};

#endif /* HEAPTRACE_UTILS_H */
