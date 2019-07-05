/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTRACE_UTILS_H
#define HEAPTRACE_UTILS_H

#include <cstdio>
#include <cstdarg>
#include <unistd.h>
#include <sys/syscall.h>

#include <string>
#include <chrono>

#ifndef _GNU_SOURCE
# define _GNU_SOURCE
#endif

namespace utils {

typedef std::chrono::duration<uint64_t>             bytes;
typedef std::chrono::duration<uint64_t, std::kilo>  kilobytes;
typedef std::chrono::duration<uint64_t, std::mega>  megabytes;
typedef std::chrono::duration<uint64_t, std::giga>  gigabytes;

static int gettid(void)
{
	return syscall(SYS_gettid);
}

std::string asprintf(const char* fmt, ...);

struct enum_table {
	const char *str;
	int val;
};

std::string mmap_prot_string(int prot);
std::string mmap_flags_string(int flags);

} // namespace utils

#endif /* HEAPTRACE_UTILS_H */
