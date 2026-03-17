cmake_minimum_required(VERSION 3.9)
project(BigMaclock)
# Option 1: Environment variable (user sets RETRO68_ROOT before running cmake)
# Or pass via command line: -DRETRO68_ROOT=/path/to/retro68
if(NOT DEFINED RETRO68_ROOT)
  set(RETRO68_ROOT $ENV{RETRO68_ROOT} CACHE PATH "Path to Retro68 toolchain")
endif()
# Set toolchain file relative to RETRO68_ROOT
set(CMAKE_TOOLCHAIN_FILE "${RETRO68_ROOT}/m68k-apple-macos/cmake/retro68.toolchain.cmake")

# Copy resource files to build directory (Rez $$read needs files in current directory)
file(COPY ${CMAKE_SOURCE_DIR}/logo_stripped.pict DESTINATION ${CMAKE_BINARY_DIR})

add_application(BigMaclock bigclock.c resources.r)

