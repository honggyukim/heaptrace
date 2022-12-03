/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTRACE_COMPILER_H
#define HEAPTRACE_COMPILER_H

#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)

#define __constructor __attribute__((constructor))
#define __destructor __attribute__((destructor))

#define __weak __attribute__((weak))
#define __visible_default __attribute__((visibility("default")))
#define __alias(func) __attribute__((alias(#func)))
#define __maybe_unused __attribute__((unused))
#define __used __attribute__((used))
#define __noreturn __attribute__((noreturn))
#define __align(n) __attribute__((aligned(n)))

#endif /* HEAPTRACE_COMPILER_H */
