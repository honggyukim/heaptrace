# Copyright (c) 2022 LG Electronics Inc.
# SPDX-License-Identifier: GPL-2.0
CC  := gcc
CXX := g++

CFLAGS   := -rdynamic -funwind-tables
CXXFLAGS := $(CFLAGS)

SRCS := sample.c factorial.c
BINS := $(patsubst %.c,%.out,$(SRCS))

all: $(BINS)

$(BINS): %.out: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINS)
