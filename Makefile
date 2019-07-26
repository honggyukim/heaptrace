# Copyright (c) 2022 LG Electronics Inc.
# SPDX-License-Identifier: GPL-2.0
ifneq ($(wildcard .config),)
  include .config
endif

prefix ?= /usr
DESTDIR := $(prefix)

ifdef CROSS_COMPILE
  CC  := $(CROSS_COMPILE)gcc
  CXX := $(CROSS_COMPILE)g++
else
  CC  ?= gcc
  CXX ?= g++
endif

CXXFLAGS := -std=c++11
ifeq ($(DEBUG), 1)
  CXXFLAGS += -O0 -g
else
  CXXFLAGS += -O2 -g
endif

LIB_CXXFLAGS := $(CXXFLAGS) -fPIC -fno-omit-frame-pointer -fvisibility=hidden
LIB_LDFLAGS  := -ldl

ifndef $(DEPTH)
# default backtrace depth is 8
DEPTH := 8
endif
LIB_CXXFLAGS += -DDEPTH=$(DEPTH)

ifeq ($(M32), 1)
  CXXFLAGS     += -m32
  LIB_CXXFLAGS += -m32
  LIB_LDFLAGS  += -m32
endif

TARGETS := heaptrace libheaptrace.so

# for libheaptrace.so
LIB_SRCS := libheaptrace.cc stacktrace.cc sighandler.cc utils.cc
LIB_OBJS := $(patsubst %.cc,%.o,$(LIB_SRCS))

# for heaptrace
HEAPTRACE_SRCS := heaptrace.cc
HEAPTRACE_OBJS := $(patsubst %.cc,%.o,$(HEAPTRACE_SRCS))


# build rule begin
all: $(TARGETS)
	$(MAKE) -C samples

heaptrace: $(HEAPTRACE_OBJS)
	$(QUIET_CXX)$(CXX) $(CXXFLAGS) -o $@ $(HEAPTRACE_OBJS)

$(LIB_OBJS): %.o: %.cc
	$(QUIET_CXX)$(CXX) $(LIB_CXXFLAGS) -c -o $@ $<

$(HEAPTRACE_OBJS): %.o: %.cc
	$(QUIET_CXX)$(CXX) $(CXXFLAGS) -c -o $@ $<

libheaptrace.so: $(LIB_OBJS)
	$(QUIET_LINK)$(CXX) -shared -o $@ $^ $(LIB_LDFLAGS)

install: all
	install -m 755 heaptrace $(DESTDIR)/bin/heaptrace
	install -m 755 libheaptrace.so $(DESTDIR)/lib/libheaptrace.so

uninstall:
	rm -f $(DESTDIR)/bin/heaptrace
	rm -f $(DESTDIR)/lib/libheaptrace.so

clean:
	rm -f heaptrace libheaptrace.so $(LIB_OBJS) $(HEAPTRACE_OBJS)
	$(MAKE) -C samples clean
