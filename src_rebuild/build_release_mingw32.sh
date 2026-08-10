#!/bin/bash
# Build REDRIVER2 release x86 with msys2 mingw32 (gcc-based, 32-bit).
# Uses the "Release_dev" config (NDEBUG + DEBUG_OPTIONS so the -mission /
# -stereo / -exitafter command-line flags still work) for a faster build.
# Usage: bash build_release_mingw32.sh

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

# Build the entire workspace with mingw32 (Release_dev config)
cd build
export PATH="$MINGW32_ROOT/bin:$PATH"
export MINGW32_INCLUDE="$MINGW32_ROOT/include"
export MINGW32_LIB="$MINGW32_ROOT/lib"
JPEG_DIR="$JPEG_DIR" \
CXXFLAGS="-Wno-narrowing -Wno-write-strings -I${SDL2_DIR} -I${OPENAL_INCLUDE}" \
CFLAGS="-Wno-narrowing -I${SDL2_DIR} -I${OPENAL_INCLUDE}" \
LDFLAGS="-L${MINGW32_ROOT}/lib" \
  mingw32-make config=release_dev_x86 "$@"