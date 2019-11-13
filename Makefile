# Copyright (c) 2022 LG Electronics Inc.
# SPDX-License-Identifier: GPL-2.0
prefix ?= /usr/local
bindir = $(prefix)/bin
libdir = $(prefix)/lib

srcdir = $(CURDIR)
# set objdir to $(O) by default (if any)
ifeq ($(objdir),)
    ifneq ($(O),)
        objdir = $(O)
    else
        objdir = $(CURDIR)
    endif
endif

ifneq ($(wildcard $(objdir)/.config),)
  include $(objdir)/.config
endif

export CC CXX LD srcdir objdir CXXFLAGS LDFLAGS

ifdef CROSS_COMPILE
  CC  := $(CROSS_COMPILE)gcc
  CXX := $(CROSS_COMPILE)g++
else
  CC  ?= gcc
  CXX ?= g++
endif

COMMON_CXXFLAGS = $(CXXFLAGS) -std=c++11 -Wno-psabi
ifeq ($(DEBUG), 1)
  COMMON_CXXFLAGS += -O0 -g
else
  COMMON_CXXFLAGS += -O2 -g
endif

LIB_CXXFLAGS := $(COMMON_CXXFLAGS) -fPIC -fno-omit-frame-pointer -fvisibility=hidden
LIB_LDFLAGS  := $(LDFLAGS) -ldl

ifndef $(DEPTH)
# default backtrace depth is 8
DEPTH := 8
endif
LIB_CXXFLAGS += -DDEPTH=$(DEPTH)

ifeq ($(M32), 1)
  COMMON_CXXFLAGS += -m32
  LIB_CXXFLAGS    += -m32
  LIB_LDFLAGS     += -m32
endif

TARGETS := heaptrace libheaptrace.so

# for libheaptrace.so
LIB_SRCS := libheaptrace.cc stacktrace.cc sighandler.cc utils.cc
LIB_OBJS := $(patsubst %.cc,$(objdir)/%.o,$(LIB_SRCS))

# for heaptrace
HEAPTRACE_SRCS := heaptrace.cc
HEAPTRACE_OBJS := $(patsubst %.cc,$(objdir)/%.o,$(HEAPTRACE_SRCS))

# build rule begin
all: $(TARGETS)
	$(MAKE) -C samples

heaptrace: $(HEAPTRACE_OBJS)
	$(QUIET_CXX)$(CXX) $(COMMON_CXXFLAGS) -o $(objdir)/$@ $(HEAPTRACE_OBJS)

$(LIB_OBJS): $(objdir)/%.o: $(srcdir)/%.cc
	$(QUIET_CXX)$(CXX) $(LIB_CXXFLAGS) -c -o $@ $<

$(HEAPTRACE_OBJS): $(objdir)/%.o: $(srcdir)/%.cc
	$(QUIET_CXX)$(CXX) $(COMMON_CXXFLAGS) -c -o $@ $<

libheaptrace.so: $(LIB_OBJS)
	$(QUIET_LINK)$(CXX) -shared -o $(objdir)/$@ $^ $(LIB_LDFLAGS)

install: all
	mkdir -p $(DESTDIR)$(bindir) $(DESTDIR)$(libdir)
	install -m 755 $(objdir)/heaptrace $(DESTDIR)$(bindir)/heaptrace
	install -m 755 $(objdir)/libheaptrace.so $(DESTDIR)$(libdir)/libheaptrace.so

uninstall:
	rm -f $(DESTDIR)$(bindir)/heaptrace
	rm -f $(DESTDIR)$(libdir)/libheaptrace.so

clean:
	rm -f $(objdir)/heaptrace $(objdir)/libheaptrace.so $(LIB_OBJS) $(HEAPTRACE_OBJS)
	$(MAKE) -C samples clean
