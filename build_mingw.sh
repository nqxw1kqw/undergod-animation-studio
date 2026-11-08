#!/usr/bin/env bash
# Build the DxLib studio app with MinGW-w64 (no Visual Studio needed).
# Verified 2026-11-08 on Linux with mingw g++ 13 + DxLib 3.25a GCC edition,
# folder "13_1_0_x86_64_w64". The resulting exe was run under Wine and the
# full studio UI rendered (see BUILD_NOTES.md).
# Usage: DXLIB_DIR=/path/to/dxlib-gcc-files ./build_mingw.sh [output.exe]
set -euo pipefail
DXLIB_DIR="${DXLIB_DIR:?Set DXLIB_DIR to the DxLib GCC folder containing DxLib.h and libDxLib.a}"
CXX="${CXX:-x86_64-w64-mingw32-g++}"
OUT="${1:-UndergodAnimationStudio.exe}"
"$CXX" -std=c++17 -O2 -mwindows -static-libgcc -static-libstdc++ \
  -I"$DXLIB_DIR" -Iinclude -Ithird_party \
  src/main.cpp src/animator.cpp src/cinematic_data.cpp src/export.cpp \
  -L"$DXLIB_DIR" \
  -lDxLib -lDxUseCLib -lDxDrawFunc -ljpeg -lpng -lzlib -ltheora_static \
  -lvorbis_static -lvorbisfile_static -lopusfile -lopus -lsilk_common -lcelt \
  -lbulletdynamics -lbulletcollision -lbulletmath -logg_static -ltiff \
  -lgdi32 -lwinmm -limm32 -lole32 -loleaut32 -luuid -lcomctl32 -lcomdlg32 \
  -lsetupapi -lws2_32 -lversion -ldxguid \
  -o "$OUT"
echo "Built: $OUT (place data/ and assets/ beside it before running)"
