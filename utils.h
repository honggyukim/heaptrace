/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTRACE_UTILS_H
#define HEAPTRACE_UTILS_H

#include <cstdio>
#include <cstdarg>
#include <sys/syscall.h>

#include <string>
#include <chrono>

namespace utils {

typedef std::chrono::duration<uint64_t>             bytes;
typedef std::chrono::duration<uint64_t, std::kilo>  kilobytes;
typedef std::chrono::duration<uint64_t, std::mega>  megabytes;
typedef std::chrono::duration<uint64_t, std::giga>  gigabytes;

static int gettid(void)
{
	return syscall(SYS_gettid);
}

static std::string asprintf(const char* fmt, ...)
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

#endif /* HEAPTRACE_UTILS_H */
