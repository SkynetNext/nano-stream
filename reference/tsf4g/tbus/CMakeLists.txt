cmake_minimum_required(VERSION 3.16)
project(tsf4g_tbus C)

add_library(tsf4g_tbus STATIC
  src/tbus.c
)

target_include_directories(tsf4g_tbus PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

# This code uses SysV shared memory APIs (Linux).
if (WIN32)
  message(WARNING "tsf4g_tbus uses SysV shared memory (sys/shm.h) and is not buildable on Windows as-is.")
endif()


