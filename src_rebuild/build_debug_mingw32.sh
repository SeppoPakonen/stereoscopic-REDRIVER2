#!/bin/bash
# Build REDRIVER2 debug x86 with msys2 mingw32 (gcc-based, 32-bit)
# Usage: bash build_debug_mingw32.sh
# Requires: msys2 with mingw-w64-i686-SDL2, openal installed
#
# Builds the whole workspace (jpeg, PsyCross, REDRIVER2) with the mingw32
# toolchain so libraries are not stale MSVC artifacts.

BASE_DIR="$(cd "${0%/*}" && pwd)"
MINGW32_ROOT=/I/msys64/mingw32

# System library paths from mingw32 packages
SDL2_DIR="$MINGW32_ROOT/include/SDL2"   # used as extra CFLAGS -I
OPENAL_INCLUDE="$MINGW32_ROOT/include"  # AL/al.h is here

# Local libjpeg source (built as part of the workspace)
JPEG_DIR="$BASE_DIR/dependencies/jpeg-9d"

# Regenerate makefiles
cd "$BASE_DIR"
JPEG_DIR="$JPEG_DIR" ./premake5 gmake2 --os=windows

# Build the entire workspace with mingw32
cd build
export PATH="$MINGW32_ROOT/bin:$PATH"
export MINGW32_INCLUDE="$MINGW32_ROOT/include"
export MINGW32_LIB="$MINGW32_ROOT/lib"
JPEG_DIR="$JPEG_DIR" \
CXXFLAGS="-Wno-narrowing -Wno-write-strings -I${SDL2_DIR} -I${OPENAL_INCLUDE}" \
CFLAGS="-Wno-narrowing -I${SDL2_DIR} -I${OPENAL_INCLUDE}" \
LDFLAGS="-L${MINGW32_ROOT}/lib" \
  mingw32-make config=debug_x86 "$@"