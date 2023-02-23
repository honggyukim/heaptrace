# Copyright (c) 2022 LG Electronics Inc.
# SPDX-License-Identifier: GPL-2.0
ifdef CROSS_COMPILE
  CC  := $(CROSS_COMPILE)gcc
  CXX := $(CROSS_COMPILE)g++
else
  CC  ?= gcc
  CXX ?= g++
endif

CFLAGS   := -rdynamic -funwind-tables
CXXFLAGS := $(CFLAGS)

SRCS := factorial.c sample.c sample_leak.c
BINS := $(patsubst %.c,%.out,$(SRCS))

all: $(BINS)

$(BINS): %.out: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINS)
