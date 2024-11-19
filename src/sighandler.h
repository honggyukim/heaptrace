/* Copyright (c) 2022 LG Electronics Inc. */
/* SPDX-License-Identifier: GPL-2.0 */
#ifndef HEAPTOP_SIGHANDLER_H
#define HEAPTOP_SIGHANDLER_H

#include <csignal>

void register_sighandler(sighandler_t handler, int signo);
void sighandler_init(void);
void size_sighandler(int);
void count_sighandler(int);

#endif /* HEAPTOP_SIGHANDLER_H */
