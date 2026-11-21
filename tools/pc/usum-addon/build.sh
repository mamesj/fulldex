#!/usr/bin/env bash
# Builds Ultra Sun and Ultra Moon's add-on for the Windows 3DS program: SDL2.dll, which goes in front of the
# program's own SDL2.dll (renamed SDL2_real.dll). See README.txt.
#
#   tools/pc/usum-addon/build.sh OUT_DIR      (OUT_DIR/SDL2.dll; the shaders and headers made on the way too)
#
# Needs MinGW-w64's C compiler (g++-mingw-w64-x86-64-posix brings it), glslangValidator (glslang-tools), the
# Vulkan headers (libvulkan-dev, or VULKAN_INC=a folder holding vulkan/ and vk_video/, such as the Vulkan SDK's
# Include) and Python 3.
set -euo pipefail
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:?which folder? tools/pc/usum-addon/build.sh OUT_DIR}"
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
CC=x86_64-w64-mingw32-gcc-posix
command -v "$CC" > /dev/null || CC=x86_64-w64-mingw32-gcc
command -v "$CC" > /dev/null || { echo "usum-addon: no MinGW-w64 C compiler (g++-mingw-w64-x86-64-posix)" >&2; exit 1; }
command -v glslangValidator > /dev/null || { echo "usum-addon: no glslangValidator (glslang-tools)" >&2; exit 1; }
# (Only Vulkan's own headers: the rest of /usr/include is Linux's, not for a Windows build.)
if [ -z "${VULKAN_INC:-}" ]; then
    [ -f /usr/include/vulkan/vulkan.h ] || { echo "usum-addon: no Vulkan headers (libvulkan-dev, or VULKAN_INC)" >&2; exit 1; }
    VULKAN_INC="$OUT/vkinc"
    rm -rf "$VULKAN_INC" && mkdir -p "$VULKAN_INC"
    cp -r /usr/include/vulkan "$VULKAN_INC/"
    [ -d /usr/include/vk_video ] && cp -r /usr/include/vk_video "$VULKAN_INC/"
fi
python3 -I "$SRC/mkglsl.py" "$SRC/usumfix.c" "$OUT"
glslangValidator -V "$OUT/usumfix.vert" -o "$OUT/usumfix.vert.spv" > /dev/null
glslangValidator -V "$OUT/usumfix.frag" -o "$OUT/usumfix.frag.spv" > /dev/null
python3 -I "$SRC/spv2h.py" "$OUT/usumfix.vert.spv" "$OUT/usumfix.frag.spv" > "$OUT/vk_shaders.h"
"$CC" -O2 -Wall -Wno-unused-function -Wno-unused-variable -shared -s -I"$OUT" -I"$SRC" -I"$VULKAN_INC" \
    -o "$OUT/SDL2.dll" "$SRC/usumfix.c" "$SRC/usumfix.def" -static-libgcc
echo "usum-addon: $OUT/SDL2.dll"
