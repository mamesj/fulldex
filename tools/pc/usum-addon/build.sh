#!/usr/bin/env bash
# Builds the 3DS program's add-on (Ultra Sun and Ultra Moon's battle buttons and X menu, X and Y's X menu, made for
# one screen). See README.txt.
#
#   tools/pc/usum-addon/build.sh OUT_DIR [windows|linux]
#     windows (the default): OUT_DIR/SDL2.dll, which goes in front of the Windows program's own SDL2.dll (renamed
#              SDL2_real.dll)
#     linux:   OUT_DIR/usumfix.so, which the launcher loads in front of the Linux program's libraries (LD_PRELOAD)
#   (the shaders and headers made on the way go in OUT_DIR too)
#
# Needs glslangValidator (glslang-tools), the Vulkan headers (libvulkan-dev, or VULKAN_INC=a folder holding vulkan/
# and vk_video/, such as the Vulkan SDK's Include) and Python 3; for Windows, MinGW-w64's C compiler
# (g++-mingw-w64-x86-64-posix brings it), for Linux the usual C compiler (gcc).
set -euo pipefail
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
OUT="${1:?which folder? tools/pc/usum-addon/build.sh OUT_DIR [windows|linux]}"
FOR="${2:-windows}"
case "$FOR" in windows|linux) ;; *) echo "usum-addon: windows or linux, not $FOR" >&2; exit 1 ;; esac
mkdir -p "$OUT"
OUT="$(cd "$OUT" && pwd)"
if [ "$FOR" = windows ]; then
    CC=x86_64-w64-mingw32-gcc-posix
    command -v "$CC" > /dev/null || CC=x86_64-w64-mingw32-gcc
    command -v "$CC" > /dev/null || { echo "usum-addon: no MinGW-w64 C compiler (g++-mingw-w64-x86-64-posix)" >&2; exit 1; }
else
    CC="${CC:-cc}"
    command -v "$CC" > /dev/null || { echo "usum-addon: no C compiler ($CC)" >&2; exit 1; }
fi
command -v glslangValidator > /dev/null || { echo "usum-addon: no glslangValidator (glslang-tools)" >&2; exit 1; }
if [ -z "${VULKAN_INC:-}" ]; then
    [ -f /usr/include/vulkan/vulkan.h ] || { echo "usum-addon: no Vulkan headers (libvulkan-dev, or VULKAN_INC)" >&2; exit 1; }
    # (For Windows only Vulkan's own headers: the rest of /usr/include is Linux's, not for a Windows build.)
    if [ "$FOR" = windows ]; then
        VULKAN_INC="$OUT/vkinc"
        rm -rf "$VULKAN_INC" && mkdir -p "$VULKAN_INC"
        cp -r /usr/include/vulkan "$VULKAN_INC/"
        [ -d /usr/include/vk_video ] && cp -r /usr/include/vk_video "$VULKAN_INC/"
    fi
fi
python3 -I "$SRC/mkglsl.py" "$SRC/usumfix.c" "$OUT"
glslangValidator -V "$OUT/usumfix.vert" -o "$OUT/usumfix.vert.spv" > /dev/null
glslangValidator -V "$OUT/usumfix.frag" -o "$OUT/usumfix.frag.spv" > /dev/null
python3 -I "$SRC/spv2h.py" "$OUT/usumfix.vert.spv" "$OUT/usumfix.frag.spv" > "$OUT/vk_shaders.h"
if [ "$FOR" = windows ]; then
    "$CC" -O2 -Wall -Wno-unused-function -Wno-unused-variable -shared -s -I"$OUT" -I"$SRC" -I"$VULKAN_INC" \
        -o "$OUT/SDL2.dll" "$SRC/usumfix.c" "$SRC/usumfix.def" -static-libgcc
    echo "usum-addon: $OUT/SDL2.dll"
else
    # (Nothing of it is seen from outside: it points the program's own imports at its functions when it loads.)
    "$CC" -O2 -Wall -Wno-unused-function -Wno-unused-variable -Wno-format-truncation -shared -fPIC -fvisibility=hidden -s \
        -I"$OUT" -I"$SRC" ${VULKAN_INC:+-I"$VULKAN_INC"} -o "$OUT/usumfix.so" "$SRC/usumfix.c" -ldl -lpthread -lm
    echo "usum-addon: $OUT/usumfix.so"
fi
