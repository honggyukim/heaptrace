# Copyright (c) 2022 LG Electronics Inc.
# SPDX-License-Identifier: GPL-2.0
CROSS_COMPILE ?= arm-linux-gnueabihf-

CC  := gcc
CXX := g++

CXXFLAGS := -std=c++14
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

clean:
	rm -f heaptrace libheaptrace.so $(LIB_OBJS) $(HEAPTRACE_OBJS)
	$(MAKE) -C samples clean
