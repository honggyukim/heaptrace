# Copyright (c) 2022 LG Electronics Inc.
# SPDX-License-Identifier: GPL-2.0
$(call allow-override,CC,$(CROSS_COMPILE)gcc)
$(call allow-override,AR,$(CROSS_COMPILE)ar)
$(call allow-override,LD,$(CROSS_COMPILE)ld)

CFLAGS   := -rdynamic -funwind-tables
CXXFLAGS := $(CFLAGS)

SRCS := sample.c factorial.c
BINS := $(patsubst %.c,%.out,$(SRCS))

all: $(BINS)

$(BINS): %.out: %.c
	$(CC) $(CFLAGS) -o $@ $<

clean:
	rm -f $(BINS)
