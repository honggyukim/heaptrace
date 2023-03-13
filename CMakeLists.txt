# SPDX-License-Identifier: GPL-2.0

cmake_minimum_required(VERSION 3.10)

project(heaptrace)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_compile_options("-Wno-psabi")

if(NOT DEFINED DEPTH)
  set(DEPTH 8)
endif(NOT DEFINED DEPTH)
add_compile_definitions(DEPTH=${DEPTH})

add_library(libheaptrace SHARED src/libheaptrace.cc src/stacktrace.cc
                                src/sighandler.cc src/utils.cc)
set_property(TARGET libheaptrace PROPERTY OUTPUT_NAME heaptrace)

add_executable(heaptrace src/heaptrace.cc)
