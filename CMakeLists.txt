cmake_minimum_required(VERSION 2.6)

set(CMAKE_C_FLAGS "-Wall -pedantic  -std=c99")
set(CMAKE_CXX_FLAGS "-Wall -pedantic")

add_definitions(-D_GNU_SOURCE)
include_directories(.)

add_executable(tesh debug.c debug-shell.c linux/hooks.c)

