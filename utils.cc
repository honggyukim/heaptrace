/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

#include "utils.h"

namespace utils {

std::string asprintf(const char* fmt, ...)
{
	va_list args;
	std::string str;
	char *ptr;
	int ret;

	va_start(args, fmt);
	ret = vasprintf(&ptr, fmt, args);
	str = ptr;
	free(ptr);
	va_end(args);
	return std::move(str);
}

} // namespace utils
