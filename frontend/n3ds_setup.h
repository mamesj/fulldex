// Put On My Switch and Put On My Phone (Windows): the FullDex Builder inside FullDex. A 3DS game's
// menu offers them; a dialog (the launcher's dialog 21, its extras from Launcher::SetupShown) walks
// the player through it:
//   Switch: the Switch's SD card found in this PC (a drive with /switch, /atmosphere or /Nintendo on
//     it), or a folder instead; then the game's code and shaders made from the player's own dump and
//     the game's folder written to /switch/<omega-nx | pokemon-alpha-sapphire> with the dump,
//     titles/<title id> and any shaders the Switch saved as missing (data/dksh/missing) built in.
//   Phone: the Android game code (<title id>.so) made into 3ds\for-phone, to copy to the phone.
//   iPhone: the game's code for iPhone and iPad put into a copy of FullDex for iPhone (an .ipa from this PC's
//     Downloads folder) in 3ds\for-iphone, for the player to sideload over the FullDex they have.
// The tools (llvm-mingw's clang and lld, devkitPro's uam, libnx, deko3d, the Switch and Android
// libraries) are not in FullDex's download: the first time, the FullDex Builder's tools are
// downloaded once into 3ds\builder\tools (curl.exe and tar.exe, both part of Windows 10 and 11).
// FULLDEX_BUILDER_TOOLS names a tools folder to use instead (a builder unzipped by hand).
// The scripts are written by FullDex each time (3ds\builder\switch.bat, android.bat), so they always
// match it. Included by n3ds.h (Windows), inside namespace N3ds.
#pragma once
#include <shellapi.h>
#include <cmath>
#include <functional>
#include <fstream>

inline const char* kBuilderToolsUrl = "https://github.com/mamesj/fulldex/releases/download/fulldex-builder-1.7.1/FullDex-Builder-1.7.1.zip";

inline const char* kSwitchBat =
    "@echo off\r\n"
    "rem Written by FullDex (Put On My Switch): the game's code, its shaders and its folder for the Switch.\r\n"
    "setlocal\r\n"
    "cd /d \"%FD_TOOLS%\" || (echo error: tools & exit /b 1)\r\n"
    "set \"OV=\"\r\n"
    "if exist \"overrides\\%FD_ID%\" set OV=--overrides \"overrides\\%FD_ID%\"\r\n"
    "set \"CC=%FD_TOOLS%\\bin\\clang.exe\"\r\n"
    "set \"AR=%FD_TOOLS%\\bin\\llvm-ar.exe\"\r\n"
    "set \"RECOMP_OPT=-O2\"\r\n"
    "set \"RECOMP_CFLAGS=--target=aarch64-none-elf -march=armv8-a+crc+crypto -mcpu=cortex-a57 -fPIE -ffreestanding -isystem switch/include\"\r\n"
    "if exist \"work-switch\" rmdir /s /q \"work-switch\"\r\n"
    "echo step: build\r\n"
    "\"3dsrecomp.exe\" port \"%FD_GAME%\" \"work-switch\" %OV%\r\n"
    "if not exist \"work-switch\\code\\librecomp.a\" (echo error: build & exit /b 1)\r\n"
    "echo step: link\r\n"
    "set \"K=switch\"\r\n"
    "\"bin\\ld.lld.exe\" -T %K%\\switch.ld -pie --no-dynamic-linker --gc-sections -z text -z now -z nodynamic-undefined-weak -z pack-relative-relocs --build-id=sha1 ^\r\n"
    "  -o work-switch\\omega-nx.elf %K%\\obj\\crti.o %K%\\obj\\crtbegin.o -u main %K%\\obj\\main.o %K%\\obj\\audio.o %K%\\obj\\shims.o %K%\\obj\\crash.o %K%\\obj\\trace_exit.o %K%\\obj\\tls.o %K%\\obj\\dkgl.o ^\r\n"
    "  --wrap=exit --wrap=_exit --wrap=abort --wrap=pthread_create --wrap=pthread_join %K%\\obj\\id-%FD_ID%.o ^\r\n"
    "  --whole-archive work-switch\\code\\librecomp.a --no-whole-archive %K%\\lib\\libomeganx.a %K%\\lib\\libdeko3d.a %K%\\lib\\libstdc++.a %K%\\lib\\libnx.a %K%\\lib\\libm.a ^\r\n"
    "  --start-group %K%\\lib\\libgcc.a %K%\\lib\\libc.a %K%\\lib\\libsysbase.a --end-group %K%\\obj\\crtend.o %K%\\obj\\crtn.o\r\n"
    "if errorlevel 1 (echo error: link & exit /b 1)\r\n"
    "\"%K%\\elf2nro.exe\" work-switch\\omega-nx.elf \"work-switch\\game.nro\" --icon=%K%\\%FD_ID%.jpg --nacp=%K%\\%FD_ID%.nacp >nul\r\n"
    "if not exist \"work-switch\\game.nro\" (echo error: nro & exit /b 1)\r\n"
    "echo step: shaders\r\n"
    "if exist \"game\\data\" rmdir /s /q \"game\\data\"\r\n"
    "mkdir \"game\\data\"\r\n"
    ">\"game\\rom.txt\" echo %FD_GAME%\r\n"
    "set \"OMEGA_WIN_COLLECT=6000\"\r\n"
    "\"game\\Pokemon ORAS.exe\" >nul 2>nul\r\n"
    "set \"OMEGA_WIN_COLLECT=\"\r\n"
    "mkdir \"work-switch\\shaders\\shaders.txt.vertex\"\r\n"
    "copy /y \"game\\data\\shaders.txt.vertex\\*.vert\" \"work-switch\\shaders\\shaders.txt.vertex\\\" >nul 2>nul\r\n"
    "\"game\\Pokemon ORAS.exe\" --switch-shader-sources \"%FD_GAME%\" \"%FD_TOOLS%\\work-switch\\shaders\" | findstr /b \"error sources\"\r\n"
    "mkdir \"work-switch\\dksh\"\r\n"
    "for %%f in (work-switch\\shaders\\glsl\\*.vert.glsl) do \"switch\\uam.exe\" -s vert -o \"work-switch\\dksh\\%%~nf.dksh\" \"%%f\" >nul 2>nul\r\n"
    "for %%f in (work-switch\\shaders\\glsl\\*.frag.glsl) do \"switch\\uam.exe\" -s frag -o \"work-switch\\dksh\\%%~nf.dksh\" \"%%f\" >nul 2>nul\r\n"
    "for %%f in (work-switch\\shaders\\glsl\\*.comp.glsl) do \"switch\\uam.exe\" -s comp -o \"work-switch\\dksh\\%%~nf.dksh\" \"%%f\" >nul 2>nul\r\n"
    "if not \"%FD_MISSING%\"==\"\" if exist \"%FD_MISSING%\\*.glsl\" (\r\n"
    "  for %%f in (\"%FD_MISSING%\\*.vert.glsl\") do \"switch\\uam.exe\" -s vert -o \"work-switch\\dksh\\%%~nf.dksh\" \"%%~ff\" >nul 2>nul\r\n"
    "  for %%f in (\"%FD_MISSING%\\*.frag.glsl\") do \"switch\\uam.exe\" -s frag -o \"work-switch\\dksh\\%%~nf.dksh\" \"%%~ff\" >nul 2>nul\r\n"
    "  for %%f in (\"%FD_MISSING%\\*.comp.glsl\") do \"switch\\uam.exe\" -s comp -o \"work-switch\\dksh\\%%~nf.dksh\" \"%%~ff\" >nul 2>nul\r\n"
    ")\r\n"
    "echo step: copy\r\n"
    "if not exist \"%FD_OUT%\\data\\dksh\" mkdir \"%FD_OUT%\\data\\dksh\"\r\n"
    "\"game\\Pokemon ORAS.exe\" --pack-dksh \"%FD_TOOLS%\\work-switch\\dksh\" \"%FD_OUT%\\data\\dksh\\shaders.pack\" | findstr /b \"packed error\"\r\n"
    "copy /y \"work-switch\\game.nro\" \"%FD_OUT%\\%FD_NAME%.nro\" >nul || (echo error: copy & exit /b 1)\r\n"
    "xcopy /e /i /y /q \"%FD_TITLES%\" \"%FD_OUT%\\titles\\%FD_ID%\" >nul\r\n"
    "if \"%FD_COPY_DUMP%\"==\"1\" (copy /y \"%FD_GAME%\" \"%FD_OUT%\\game.3ds\" >nul || (echo error: copy & exit /b 1))\r\n"
    "rmdir /s /q \"work-switch\"\r\n"
    "echo done\r\n";

inline const char* kAndroidBat =
    "@echo off\r\n"
    "rem Written by FullDex (Put On My Phone): the game's code for Android phones.\r\n"
    "setlocal\r\n"
    "cd /d \"%FD_TOOLS%\" || (echo error: tools & exit /b 1)\r\n"
    "set \"OV=\"\r\n"
    "if exist \"overrides\\%FD_ID%\" set OV=--overrides \"overrides\\%FD_ID%\"\r\n"
    "set \"CC=%FD_TOOLS%\\bin\\clang.exe\"\r\n"
    "set \"RECOMP_CFLAGS=--target=aarch64-linux-android30 -isystem android/include/aarch64-linux-android -isystem android/include\"\r\n"
    "set \"RECOMP_OPT=-O2\"\r\n"
    "set \"RECOMP_LDFLAGS=--target=aarch64-linux-android30 -fuse-ld=lld -nostdlib -Wl,-z,max-page-size=16384 -s\"\r\n"
    "if exist \"work-android\" rmdir /s /q \"work-android\"\r\n"
    "echo step: build\r\n"
    "\"3dsrecomp.exe\" build \"%FD_GAME%\" \"work-android\" %OV%\r\n"
    "if errorlevel 1 (echo error: build & exit /b 1)\r\n"
    "echo step: copy\r\n"
    "if not exist \"%FD_OUT%\" mkdir \"%FD_OUT%\"\r\n"
    "set \"MADE=\"\r\n"
    "for %%f in (work-android\\*.so work-android\\*.dll) do (copy /y \"%%f\" \"%FD_OUT%\\%FD_ID%.so\" >nul && set \"MADE=1\")\r\n"
    "if not defined MADE (echo error: build & exit /b 1)\r\n"
    "rmdir /s /q \"work-android\"\r\n"
    "echo done\r\n";


// Put On My iPhone: the game's code for iPhone and iPad, built with the same clang and lld for Apple's arm64 (no
// Apple SDK: these two headers and ios_rt.c are all the code needs), then put into a copy of FullDex for iPhone
// (an .ipa the player downloaded) as Frameworks/omega_<title id>.framework, which the sideloading tool signs with
// the rest. FullDex for iPhone's 3DS engine finds it there and runs the game's code instead of interpreting it.
inline const char* kIosBat =
    "@echo off\r\n"
    "rem Written by FullDex (Put On My iPhone): the game's code for iPhone and iPad, put into FullDex for iPhone.\r\n"
    "setlocal\r\n"
    "cd /d \"%FD_TOOLS%\" || (echo error: tools & exit /b 1)\r\n"
    "set \"OV=\"\r\n"
    "if exist \"overrides\\%FD_ID%\" set OV=--overrides \"overrides\\%FD_ID%\"\r\n"
    "rem (lld links Apple's files when it is called ld64.lld)\r\n"
    "if not exist \"bin\\ld64.lld.exe\" copy /y \"bin\\ld.lld.exe\" \"bin\\ld64.lld.exe\" >nul\r\n"
    "set \"CC=%FD_TOOLS%\\bin\\clang.exe\"\r\n"
    "set \"RECOMP_CFLAGS=--target=arm64-apple-ios14.0 -fPIC -ffreestanding -isystem ios/include\"\r\n"
    "set \"RECOMP_OPT=-O2\"\r\n"
    "set \"RECOMP_LDFLAGS=--target=arm64-apple-ios14.0 -fuse-ld=lld -nostdlib -Wl,-install_name,@rpath/omega_%FD_ID%.framework/omega_%FD_ID% ios\\ios_rt.o\"\r\n"
    "if exist \"work-ios\" rmdir /s /q \"work-ios\"\r\n"
    "echo step: build\r\n"
    "\"bin\\clang.exe\" --target=arm64-apple-ios14.0 -O2 -fPIC -ffreestanding -fno-builtin -isystem ios/include -c ios\\ios_rt.c -o ios\\ios_rt.o || (echo error: build & exit /b 1)\r\n"
    "\"3dsrecomp.exe\" build \"%FD_GAME%\" \"work-ios\" %OV%\r\n"
    "if errorlevel 1 (echo error: build & exit /b 1)\r\n"
    "set \"MADE=\"\r\n"
    "for %%f in (work-ios\\*.dll work-ios\\*.dylib work-ios\\*.so) do (copy /y \"%%f\" \"ios\\omega_%FD_ID%\" >nul && set \"MADE=1\")\r\n"
    "if not defined MADE (echo error: build & exit /b 1)\r\n"
    "echo step: copy\r\n"
    "if not exist \"%FD_OUT%\" mkdir \"%FD_OUT%\"\r\n"
    "powershell.exe -NoProfile -ExecutionPolicy Bypass -File \"ios\\ipa.ps1\"\r\n"
    "if errorlevel 3 (echo error: noengine & exit /b 1)\r\n"
    "if errorlevel 2 (echo error: noipa & exit /b 1)\r\n"
    "if errorlevel 1 (echo error: ipa & exit /b 1)\r\n"
    "rmdir /s /q \"work-ios\"\r\n"
    "echo done\r\n";
inline const char* kIosStringH =
    "/* omega-nx, Put On My iPhone: string.h for the game's code built for iOS on a PC (no Apple SDK).\r\n"
    " * Only what the recompiled code uses; the functions are in ios_rt.c, inside the game's library. */\r\n"
    "#pragma once\r\n"
    "#include <stddef.h>\r\n"
    "void *memcpy(void *restrict, const void *restrict, size_t);\r\n"
    "void *memmove(void *, const void *, size_t);\r\n"
    "void *memset(void *, int, size_t);\r\n"
    "int memcmp(const void *, const void *, size_t);\r\n";
inline const char* kIosMathH =
    "/* omega-nx, Put On My iPhone: math.h for the game's code built for iOS on a PC (no Apple SDK).\r\n"
    " * The compiler's own builtins: each is one arm64 instruction, no library call. */\r\n"
    "#pragma once\r\n"
    "#define sqrtf(x) __builtin_sqrtf(x)\r\n"
    "#define sqrt(x) __builtin_sqrt(x)\r\n"
    "#define fabsf(x) __builtin_fabsf(x)\r\n"
    "#define fabs(x) __builtin_fabs(x)\r\n"
    "#define floorf(x) __builtin_floorf(x)\r\n"
    "#define floor(x) __builtin_floor(x)\r\n"
    "#define ceilf(x) __builtin_ceilf(x)\r\n"
    "#define ceil(x) __builtin_ceil(x)\r\n"
    "#define truncf(x) __builtin_truncf(x)\r\n"
    "#define trunc(x) __builtin_trunc(x)\r\n"
    "#define fmaf(x, y, z) __builtin_fmaf(x, y, z)\r\n"
    "#define fma(x, y, z) __builtin_fma(x, y, z)\r\n"
    "#define isnan(x) __builtin_isnan(x)\r\n"
    "#define isinf(x) __builtin_isinf(x)\r\n"
    "#define signbit(x) __builtin_signbit(x)\r\n"
    "#define copysignf(x, y) __builtin_copysignf(x, y)\r\n"
    "#define copysign(x, y) __builtin_copysign(x, y)\r\n"
    "#define NAN __builtin_nanf(\"\")\r\n"
    "#define INFINITY __builtin_inff()\r\n";
inline const char* kIosRtC =
    "/* omega-nx, Put On My iPhone: the few C library functions a compiler may call from the game's code,\r\n"
    " * inside the game's library itself, so it needs nothing from iOS (no SDK on the PC that builds it).\r\n"
    " * Built with -fno-builtin, so these loops are not turned back into calls to themselves. */\r\n"
    "#include <stddef.h>\r\n"
    "#include <stdint.h>\r\n"
    "#define HIDDEN __attribute__((visibility(\"hidden\")))\r\n"
    "\r\n"
    "HIDDEN void *memcpy(void *restrict d, const void *restrict s, size_t n)\r\n"
    "{\r\n"
    "    unsigned char *dp = d; const unsigned char *sp = s;\r\n"
    "    while (n >= 8) { uint64_t v; __builtin_memcpy_inline(&v, sp, 8); __builtin_memcpy_inline(dp, &v, 8); dp += 8; sp += 8; n -= 8; }\r\n"
    "    while (n--) *dp++ = *sp++;\r\n"
    "    return d;\r\n"
    "}\r\n"
    "HIDDEN void *memmove(void *d, const void *s, size_t n)\r\n"
    "{\r\n"
    "    unsigned char *dp = d; const unsigned char *sp = s;\r\n"
    "    if (dp == sp || n == 0) return d;\r\n"
    "    if (dp < sp || dp >= sp + n) return memcpy(d, s, n);\r\n"
    "    dp += n; sp += n;\r\n"
    "    while (n--) *--dp = *--sp;\r\n"
    "    return d;\r\n"
    "}\r\n"
    "HIDDEN void *memset(void *d, int c, size_t n)\r\n"
    "{\r\n"
    "    unsigned char *dp = d; const uint64_t v = 0x0101010101010101ull * (unsigned char)c;\r\n"
    "    while (n >= 8) { __builtin_memcpy_inline(dp, &v, 8); dp += 8; n -= 8; }\r\n"
    "    while (n--) *dp++ = (unsigned char)c;\r\n"
    "    return d;\r\n"
    "}\r\n"
    "HIDDEN void bzero(void *d, size_t n) { memset(d, 0, n); }\r\n"
    "HIDDEN int memcmp(const void *a, const void *b, size_t n)\r\n"
    "{\r\n"
    "    const unsigned char *x = a, *y = b;\r\n"
    "    for (; n; n--, x++, y++) if (*x != *y) return *x - *y;\r\n"
    "    return 0;\r\n"
    "}\r\n";
inline const char* kIosIpaPs1 =
    "# Written by FullDex (Put On My iPhone): the game's code put into a copy of FullDex for iPhone (an .ipa).\r\n"
    "# The .ipa's own files are copied byte for byte (their Unix permissions kept: the app's program must stay\r\n"
    "# executable) and the framework is added after them, so nothing else in it changes.\r\n"
    "$ErrorActionPreference = 'Stop'\r\n"
    "Add-Type -AssemblyName System.IO.Compression\r\n"
    "Add-Type -AssemblyName System.IO.Compression.FileSystem\r\n"
    "Add-Type -TypeDefinition @'\r\n"
    "using System; using System.IO; using System.IO.Compression; using System.Text; using System.Collections.Generic;\r\n"
    "public static class FullDexIpa {\r\n"
    "  static uint[] T = Table();\r\n"
    "  static uint[] Table() { var t = new uint[256]; for (uint i = 0; i < 256; i++) { uint c = i; for (int k = 0; k < 8; k++) c = (c & 1) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1; t[i] = c; } return t; }\r\n"
    "  static uint Crc(byte[] b) { uint c = 0xFFFFFFFFu; foreach (byte x in b) c = T[(c ^ x) & 0xFF] ^ (c >> 8); return ~c; }\r\n"
    "  static void U16(Stream s, int v) { s.WriteByte((byte)v); s.WriteByte((byte)(v >> 8)); }\r\n"
    "  static void U32(Stream s, uint v) { U16(s, (int)(v & 0xFFFF)); U16(s, (int)(v >> 16)); }\r\n"
    "  static int R16(byte[] b, long o) { return b[o] | (b[o + 1] << 8); }\r\n"
    "  static uint R32(byte[] b, long o) { return (uint)(R16(b, o) | (R16(b, o + 2) << 16)); }\r\n"
    "  // Returns the app's folder in the archive (Payload/X.app/), or throws.\r\n"
    "  public static string AppDir(string ipa) {\r\n"
    "    using (var z = ZipFile.OpenRead(ipa)) foreach (var e in z.Entries) if (System.Text.RegularExpressions.Regex.IsMatch(e.FullName, \"^Payload/[^/]+\\\\.app/Info\\\\.plist$\")) return e.FullName.Substring(0, e.FullName.Length - 10);\r\n"
    "    throw new Exception(\"no app in \" + ipa);\r\n"
    "  }\r\n"
    "  // src's entries copied as they are (those under drop left out) and files added: names, contents, Unix modes.\r\n"
    "  public static void Add(string src, string dst, string drop, string[] names, string[] paths, int[] modes) {\r\n"
    "    byte[] a = File.ReadAllBytes(src);\r\n"
    "    long e = -1;\r\n"
    "    for (long i = a.Length - 22; i >= 0 && i >= a.Length - 65557; i--) if (R32(a, i) == 0x06054b50) { e = i; break; }\r\n"
    "    if (e < 0) throw new Exception(\"not a zip\");\r\n"
    "    uint cdSize = R32(a, e + 12), cdOff = R32(a, e + 16);\r\n"
    "    if (cdOff == 0xFFFFFFFFu) throw new Exception(\"zip64 is not handled\");\r\n"
    "    var keep = new List<byte[]>();\r\n"
    "    for (long o = cdOff; o < cdOff + cdSize; ) {\r\n"
    "      if (R32(a, o) != 0x02014b50) throw new Exception(\"bad central directory\");\r\n"
    "      int n = R16(a, o + 28), x = R16(a, o + 30), c = R16(a, o + 32);\r\n"
    "      long len = 46 + n + x + c;\r\n"
    "      string name = Encoding.UTF8.GetString(a, (int)(o + 46), n);\r\n"
    "      if (!name.StartsWith(drop)) { var r = new byte[len]; Array.Copy(a, o, r, 0, len); keep.Add(r); }\r\n"
    "      o += len;\r\n"
    "    }\r\n"
    "    int time = (DateTime.Now.Hour << 11) | (DateTime.Now.Minute << 5) | (DateTime.Now.Second / 2);\r\n"
    "    int date = ((DateTime.Now.Year - 1980) << 9) | (DateTime.Now.Month << 5) | DateTime.Now.Day;\r\n"
    "    using (var f = new FileStream(dst, FileMode.Create, FileAccess.Write)) {\r\n"
    "      f.Write(a, 0, (int)cdOff);\r\n"
    "      var added = new List<byte[]>();\r\n"
    "      for (int i = 0; i < names.Length; i++) {\r\n"
    "        byte[] data = File.ReadAllBytes(paths[i]), nm = Encoding.UTF8.GetBytes(names[i]), comp;\r\n"
    "        using (var m = new MemoryStream()) { using (var d = new DeflateStream(m, CompressionLevel.Optimal, true)) d.Write(data, 0, data.Length); comp = m.ToArray(); }\r\n"
    "        uint crc = Crc(data), at = (uint)f.Position;\r\n"
    "        U32(f, 0x04034b50); U16(f, 20); U16(f, 0); U16(f, 8); U16(f, time); U16(f, date);\r\n"
    "        U32(f, crc); U32(f, (uint)comp.Length); U32(f, (uint)data.Length); U16(f, nm.Length); U16(f, 0);\r\n"
    "        f.Write(nm, 0, nm.Length); f.Write(comp, 0, comp.Length);\r\n"
    "        using (var m = new MemoryStream()) {\r\n"
    "          U32(m, 0x02014b50); U16(m, 0x031E); U16(m, 20); U16(m, 0); U16(m, 8); U16(m, time); U16(m, date);\r\n"
    "          U32(m, crc); U32(m, (uint)comp.Length); U32(m, (uint)data.Length); U16(m, nm.Length); U16(m, 0); U16(m, 0);\r\n"
    "          U16(m, 0); U16(m, 0); U32(m, (uint)modes[i] << 16); U32(m, at); m.Write(nm, 0, nm.Length);\r\n"
    "          added.Add(m.ToArray());\r\n"
    "        }\r\n"
    "      }\r\n"
    "      uint cd = (uint)f.Position; int count = 0;\r\n"
    "      foreach (var r in keep) { f.Write(r, 0, r.Length); count++; }\r\n"
    "      foreach (var r in added) { f.Write(r, 0, r.Length); count++; }\r\n"
    "      uint size = (uint)f.Position - cd;\r\n"
    "      U32(f, 0x06054b50); U16(f, 0); U16(f, 0); U16(f, count); U16(f, count); U32(f, size); U32(f, cd); U16(f, 0);\r\n"
    "    }\r\n"
    "  }\r\n"
    "}\r\n"
    "'@\r\n"
    "$name = \"omega_$($env:FD_ID)\"\r\n"
    "$lib = Join-Path $env:FD_TOOLS \"ios\\$name\"\r\n"
    "$plist = Join-Path $env:FD_TOOLS \"ios\\Info-$($env:FD_ID).plist\"\r\n"
    "$out = $env:FD_IPA_OUT\r\n"
    "$found = $null; $any = $false\r\n"
    "foreach ($c in ($env:FD_IPAS -split '\\|')) {\r\n"
    "  if (-not $c -or -not (Test-Path -LiteralPath $c)) { continue }\r\n"
    "  $any = $true\r\n"
    "  try { $z = [System.IO.Compression.ZipFile]::OpenRead($c) } catch { continue }\r\n"
    "  $ok = @($z.Entries | Where-Object { $_.FullName -match '^Payload/[^/]+\\.app/Frameworks/MoltenVK\\.framework/MoltenVK$' }).Count -gt 0\r\n"
    "  $z.Dispose()\r\n"
    "  if ($ok) { $found = $c; break }\r\n"
    "}\r\n"
    "if (-not $found) { if ($any) { exit 3 } else { exit 2 } }\r\n"
    "Write-Output \"ipa: $found\"\r\n"
    "$tmp = \"$out.new\"\r\n"
    "try {\r\n"
    "  $app = [FullDexIpa]::AppDir($found)\r\n"
    "  $dir = \"${app}Frameworks/$name.framework/\"\r\n"
    "  [FullDexIpa]::Add($found, $tmp, $dir, @(\"$dir$name\", \"${dir}Info.plist\"), @($lib, $plist), @(0x81ED, 0x81A4))\r\n"
    "  Move-Item -LiteralPath $tmp -Destination $out -Force\r\n"
    "} catch {\r\n"
    "  Write-Output \"error: $_\"\r\n"
    "  if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Force }\r\n"
    "  exit 1\r\n"
    "}\r\n"
    "exit 0\r\n";
inline std::string IosFrameworkPlist(const std::string& id)
{
    const std::string n = "omega_" + id;
    return "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n"
           "<!DOCTYPE plist PUBLIC \"-//Apple//DTD PLIST 1.0//EN\" \"http://www.apple.com/DTDs/PropertyList-1.0.dtd\">\n"
           "<plist version=\"1.0\"><dict>\n"
           "<key>CFBundleDevelopmentRegion</key><string>en</string>\n"
           "<key>CFBundleExecutable</key><string>" + n + "</string>\n"
           "<key>CFBundleIdentifier</key><string>io.github.mamesj.pokemonds.omega-" + id + "</string>\n"
           "<key>CFBundleInfoDictionaryVersion</key><string>6.0</string>\n"
           "<key>CFBundleName</key><string>" + n + "</string>\n"
           "<key>CFBundlePackageType</key><string>FMWK</string>\n"
           "<key>CFBundleShortVersionString</key><string>1.0</string>\n"
           "<key>CFBundleVersion</key><string>1</string>\n"
           "<key>CFBundleSupportedPlatforms</key><array><string>iPhoneOS</string></array>\n"
           "<key>MinimumOSVersion</key><string>14.0</string>\n"
           "</dict></plist>\n";
}
// The copies of FullDex for iPhone it could add the game to, best first: the one it made before (so a second game
// is added beside the first), then any .ipa of FullDex's in this PC's Downloads folder or FullDex's own, newest first.
inline std::string IosIpaCandidates(const std::string& appDir, const std::string& out)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    std::vector<std::pair<fs::file_time_type, std::string>> found;
    std::vector<std::string> dirs = { appDir };
    if (const char* up = getenv("USERPROFILE")) if (*up) dirs.push_back(std::string(up) + "/Downloads");
    for (const std::string& d : dirs)
        for (fs::directory_iterator it(W(d), ec), end; !ec && it != end; it.increment(ec))
        {
            if (!it->is_regular_file(ec)) continue;
            std::string fn = it->path().filename().u8string();
            for (char& ch : fn) ch = (char)tolower((unsigned char)ch);
            if (!EndsWith(fn, ".ipa") || (fn.find("fulldex") == std::string::npos && fn.find("pokemon-ds-wide") == std::string::npos)) continue;
            found.push_back({ it->last_write_time(ec), it->path().u8string() });
        }
    std::sort(found.begin(), found.end(), [](const auto& a, const auto& b) { return a.first > b.first; });
    std::string list = out + "/" + "FullDex for iPhone (3DS games built in).ipa";
    for (const auto& f : found) list += "|" + f.second;
    return list;
}

// A drive that looks like a Switch's SD card.
struct SdCard { std::string root, label; double freeGb = 0; };
inline std::vector<SdCard> FindSdCards()
{
    std::vector<SdCard> out;
    wchar_t all[512] = {};
    const DWORD n = GetLogicalDriveStringsW(511, all);
    for (const wchar_t* d = all; n && *d; d += wcslen(d) + 1)
    {
        const UINT type = GetDriveTypeW(d);
        if (type != DRIVE_REMOVABLE && type != DRIVE_FIXED) continue;
        if (!_wcsicmp(d, L"C:\\")) continue;
        const std::wstring root = d;
        auto has = [&](const wchar_t* f) { const DWORD a = GetFileAttributesW((root + f).c_str()); return a != INVALID_FILE_ATTRIBUTES && (a & FILE_ATTRIBUTE_DIRECTORY); };
        if (!has(L"switch") && !has(L"atmosphere") && !has(L"Nintendo")) continue;
        SdCard c;
        char r[8] = {};
        WideCharToMultiByte(CP_UTF8, 0, root.c_str(), 2, r, sizeof r, nullptr, nullptr);
        c.root = r; // "E:"
        wchar_t label[128] = {};
        if (GetVolumeInformationW(d, label, 127, nullptr, nullptr, nullptr, nullptr, 0) && label[0])
        {
            char l[256] = {};
            WideCharToMultiByte(CP_UTF8, 0, label, -1, l, sizeof l, nullptr, nullptr);
            c.label = l;
        }
        ULARGE_INTEGER free {};
        if (GetDiskFreeSpaceExW(d, &free, nullptr, nullptr)) c.freeGb = (double)free.QuadPart / (1024.0 * 1024 * 1024);
        out.push_back(c);
    }
    return out;
}

struct SetupJob
{
    std::mutex m;
    LooseThread th;
    std::atomic<bool> running { false };
    bool open = false;   // the dialog shown (Keep Using FullDex hides it while it works)
    bool told = true;    // the one note at its end given
    int kind = 0;        // 1 Switch, 2 phone, 3 iPhone
    int screen = 0;      // 0 where it goes, 1 working, 2 done, 3 failed, 4 the Switch's how-to (no PC here)
    int step = 0;        // 0 tools, 1 build, 2 shaders, 3 copy
    bool needTools = false, toFolder = false, dumpCopied = false;
    int missing = 0;     // shaders the Switch saved as missing, built in
    int done = 0, total = 0; // (the build: the game's code compiled so far, of how many)
    std::string appDir, id, code, name, rom, out, drive, error;
    std::vector<SdCard> cards;
    std::chrono::steady_clock::time_point started {}, stepAt {}, lastScan {};
    HANDLE job = nullptr; // (the build's processes, all ended together by Stop)
};
inline SetupJob& SJ() { static SetupJob j; return j; }

inline std::string SetupFolderName(const std::string& code) { return SwitchFolder(code); }
inline std::string SetupToolsDir(const std::string& appDir)
{
    if (const char* t = getenv("FULLDEX_BUILDER_TOOLS")) if (*t) return t;
    return appDir + "/3ds/builder/tools";
}
// The tools from an older FullDex (no stamp, or another download's) are fetched again, so a fix in
// the Builder reaches everyone who set up before it. A folder named by FULLDEX_BUILDER_TOOLS is used as it is.
inline std::string SetupToolsStamp(const std::string& appDir) { return SetupToolsDir(appDir) + "/fulldex-tools.txt"; }
inline bool SetupHaveTools(const std::string& appDir)
{
    std::error_code ec;
    if (!std::filesystem::is_regular_file(W(SetupToolsDir(appDir) + "/3dsrecomp.exe"), ec)) return false;
    if (const char* t = getenv("FULLDEX_BUILDER_TOOLS")) if (*t) return true;
    std::ifstream f(std::filesystem::path(W(SetupToolsStamp(appDir))));
    std::string got;
    std::getline(f, got);
    return got == kBuilderToolsUrl;
}
inline std::string SetupShortName(const std::string& name) { return SetupName(name); }
inline std::string Slashes(std::string p) { for (char& ch : p) if (ch == '/') ch = '\\'; return p; }

// Runs a command hidden, its output line by line to take; its exit code (-1: could not start it).
inline int SetupRun(const std::wstring& cmd, const std::string& dir, const std::function<void(const std::string&)>& take, HANDLE job = nullptr)
{
    SECURITY_ATTRIBUTES sa { sizeof sa, nullptr, TRUE };
    HANDLE rd = nullptr, wr = nullptr;
    if (!CreatePipe(&rd, &wr, &sa, 0)) return -1;
    SetHandleInformation(rd, HANDLE_FLAG_INHERIT, 0);
    STARTUPINFOW si {};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdOutput = wr;
    si.hStdError = wr;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    PROCESS_INFORMATION pi {};
    std::wstring c = cmd;
    const BOOL ok = CreateProcessW(nullptr, c.data(), nullptr, nullptr, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED, nullptr, dir.empty() ? nullptr : W(dir).c_str(), &si, &pi);
    CloseHandle(wr);
    if (!ok) { CloseHandle(rd); return -1; }
    if (job) AssignProcessToJobObject(job, pi.hProcess);
    ResumeThread(pi.hThread);
    std::string line;
    char buf[512];
    DWORD got = 0;
    while (ReadFile(rd, buf, sizeof buf, &got, nullptr) && got > 0)
        for (DWORD i = 0; i < got; i++)
        {
            if (buf[i] == '\n') { if (!line.empty() && line.back() == '\r') line.pop_back(); take(line); line.clear(); }
            else line += buf[i];
        }
    if (!line.empty()) take(line);
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 1;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hProcess);
    CloseHandle(pi.hThread);
    CloseHandle(rd);
    return (int)code;
}

// The Builder's tools, downloaded once: false and why when they could not be.
inline bool SetupGetTools(const std::string& appDir, std::string& why)
{
    namespace fs = std::filesystem;
    std::error_code ec;
    const std::string base = appDir + "/3ds/builder", zip = base + "/tools-download.zip", unz = base + "/unzipped";
    fs::create_directories(W(base), ec);
    fs::remove_all(W(unz), ec);
    auto log = [](const std::string& l) { fprintf(stderr, "[3ds setup] %s\n", l.c_str()); };
    int code = SetupRun(L"curl.exe -L --fail --silent --show-error -o \"" + W(zip) + L"\" \"" + W(kBuilderToolsUrl) + L"\"", base, log);
    if (code != 0) { why = "Couldn't download the tools. Check the internet connection and try again."; return false; }
    fs::create_directories(W(unz), ec);
    code = SetupRun(L"tar.exe -xf \"" + W(zip) + L"\" -C \"" + W(unz) + L"\"", base, log);
    if (code != 0) { why = "Couldn't unpack the tools."; return false; }
    // (the tools folder is the one holding 3dsrecomp.exe, wherever the zip put it)
    fs::path found;
    for (fs::recursive_directory_iterator it(W(unz), ec), end; !ec && it != end; it.increment(ec))
        if (it->path().filename() == "3dsrecomp.exe") { found = it->path().parent_path(); break; }
    if (found.empty()) { why = "The tools download was not complete. Try again."; return false; }
    fs::remove_all(W(base + "/tools"), ec);
    fs::rename(found, W(base + "/tools"), ec);
    if (ec) { why = "Couldn't put the tools in place."; return false; }
    fs::remove_all(W(unz), ec);
    fs::remove(W(zip), ec);
    { std::ofstream f(std::filesystem::path(W(SetupToolsStamp(appDir)))); f << kBuilderToolsUrl << "\n"; }
    return SetupHaveTools(appDir);
}

inline void SetupRefreshCards()
{
    SetupJob& j = SJ();
    j.cards = FindSdCards();
    j.lastScan = std::chrono::steady_clock::now();
}

// A game's menu: Put On My Switch (kind 1), Put On My Phone (2) or Put On My iPhone (3). Opens the dialog where it stands.
inline void SetupOpen(const std::string& appDir, const Launcher::Game& g, int kind)
{
    SetupJob& j = SJ();
    std::lock_guard<std::mutex> l(j.m);
    j.open = true;
    if (j.running) return; // (working on one already: shown as it goes)
    j.kind = kind;
    j.screen = kind == 1 ? 0 : 1;
    j.appDir = appDir;
    j.id = IdOf(g);
    j.code = g.code;
    j.name = SetupShortName(g.name);
    j.rom = g.rom;
    j.error.clear();
    j.cards = FindSdCards();
    j.lastScan = std::chrono::steady_clock::now();
}
inline void SetupStart(const std::string& drive, bool toFolder);
inline void SetupOpenAndGo(const std::string& appDir, const Launcher::Game& g, int kind)
{
    SetupOpen(appDir, g, kind);
    if (kind != 1 && !SJ().running) SetupStart("", false);
}
inline bool SetupOpenNow() { return SJ().open; }

inline void SetupStart(const std::string& drive, bool toFolder)
{
    SetupJob& j = SJ();
    if (j.running) return;
    if (j.th.joinable()) j.th.join();
    {
        std::lock_guard<std::mutex> l(j.m);
        j.drive = drive;
        j.toFolder = toFolder;
        j.screen = 1;
        j.step = 0;
        j.error.clear();
        j.missing = 0;
        j.done = j.total = 0;
        j.dumpCopied = false;
        j.told = false;
        j.needTools = !SetupHaveTools(j.appDir);
        j.out = j.kind == 3 ? j.appDir + "/3ds/for-iphone"
              : j.kind == 2 ? j.appDir + "/3ds/for-phone"
              : toFolder ? j.appDir + "/3ds/for-switch/" + SetupFolderName(j.code)
              : drive + "/switch/" + SetupFolderName(j.code);
        j.started = j.stepAt = std::chrono::steady_clock::now();
        if (!j.job)
        {
            j.job = CreateJobObjectW(nullptr, nullptr);
            JOBOBJECT_EXTENDED_LIMIT_INFORMATION li {};
            li.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
            if (j.job) SetInformationJobObject(j.job, JobObjectExtendedLimitInformation, &li, sizeof li);
        }
    }
    j.running = true;
    j.th = std::thread([]() {
        SetupJob& j = SJ();
        std::string appDir, id, code, rom, out, name;
        int kind;
        bool needTools, toFolder;
        {
            std::lock_guard<std::mutex> l(j.m);
            appDir = j.appDir; id = j.id; code = j.code; rom = j.rom; out = j.out; name = j.name; kind = j.kind; needTools = j.needTools; toFolder = j.toFolder;
        }
        auto fail = [&](const std::string& why) { std::lock_guard<std::mutex> l(j.m); if (j.error.empty()) j.error = why; };
        auto step = [&](int s) { std::lock_guard<std::mutex> l(j.m); j.step = s; j.stepAt = std::chrono::steady_clock::now(); };
        namespace fs = std::filesystem;
        std::error_code ec;
        std::string why;
        if (needTools && !SetupGetTools(appDir, why)) fail(why);
        const std::string tools = SetupToolsDir(appDir);
        if (j.error.empty())
        {
            // (room for it: on the SD card the dump and about 100 MB, else 300 MB here)
            const double dumpGb = (double)fs::file_size(W(rom), ec) / (1024.0 * 1024 * 1024);
            ULARGE_INTEGER free {};
            const std::string where = kind == 1 && !toFolder ? out.substr(0, 2) + "\\" : appDir;
            fs::create_directories(W(out), ec);
            if (GetDiskFreeSpaceExW(W(where).c_str(), &free, nullptr, nullptr))
            {
                const double needGb = (kind == 1 ? dumpGb : 0.0) + 0.3, haveGb = (double)free.QuadPart / (1024.0 * 1024 * 1024);
                if (haveGb < needGb)
                {
                    char t[160];
                    snprintf(t, sizeof t, "Not enough space: it needs %.1f GB free and there is %.1f GB.", needGb, haveGb);
                    fail(t);
                }
            }
        }
        if (j.error.empty())
        {
            const std::string bat = appDir + "/3ds/builder/" + (kind == 1 ? "switch.bat" : kind == 3 ? "ios.bat" : "android.bat");
            fs::create_directories(W(appDir + "/3ds/builder"), ec);
            if (FILE* f = _wfopen(W(bat).c_str(), L"wb")) { fputs(kind == 1 ? kSwitchBat : kind == 3 ? kIosBat : kAndroidBat, f); fclose(f); }
            if (kind == 3)
            {
                // (the files the iPhone build needs, written each time as the scripts are)
                fs::create_directories(W(tools + "/ios/include"), ec);
                const std::pair<std::string, std::string> files[] = { { "ios/include/string.h", kIosStringH }, { "ios/include/math.h", kIosMathH },
                    { "ios/ios_rt.c", kIosRtC }, { "ios/ipa.ps1", kIosIpaPs1 }, { "ios/Info-" + id + ".plist", IosFrameworkPlist(id) } };
                for (const auto& f : files)
                    if (FILE* o = _wfopen(W(tools + "/" + f.first).c_str(), L"wb")) { fputs(f.second.c_str(), o); fclose(o); }
                SetEnvironmentVariableW(L"FD_IPAS", W(Slashes(IosIpaCandidates(appDir, out))).c_str());
                SetEnvironmentVariableW(L"FD_IPA_OUT", W(Slashes(out + "/FullDex for iPhone (3DS games built in).ipa")).c_str());
            }
            // (the shaders the Switch saved as missing, when the game is on this card already)
            const std::string missing = out + "/data/dksh/missing";
            int nMissing = 0;
            for (fs::directory_iterator it(W(missing), ec), end; !ec && it != end; it.increment(ec))
                if (it->path().extension() == ".glsl") nMissing++;
            // (the dump: copied unless the same one is there)
            const bool copyDump = kind == 1 && fs::file_size(W(out + "/game.3ds"), ec) != fs::file_size(W(rom), ec);
            SetEnvironmentVariableW(L"FD_TOOLS", W(Slashes(tools)).c_str());
            SetEnvironmentVariableW(L"FD_GAME", W(Slashes(rom)).c_str());
            SetEnvironmentVariableW(L"FD_OUT", W(Slashes(out)).c_str());
            SetEnvironmentVariableW(L"FD_ID", W(id).c_str());
            SetEnvironmentVariableW(L"FD_NAME", W(ProgramName(code)).c_str());
            SetEnvironmentVariableW(L"FD_TITLES", W(Slashes(ProgramDir(appDir) + "/titles/" + id)).c_str());
            SetEnvironmentVariableW(L"FD_MISSING", W(nMissing ? Slashes(missing) : std::string()).c_str());
            SetEnvironmentVariableW(L"FD_COPY_DUMP", copyDump ? L"1" : L"0");
            step(1);
            const int rc = SetupRun(L"cmd.exe /d /c \"\"" + W(Slashes(bat)) + L"\"\"", appDir, [&](const std::string& l) {
                fprintf(stderr, "[3ds setup] %s\n", l.c_str());
                int a = 0, b = 0;
                if (sscanf(l.c_str(), "compiled %d of %d", &a, &b) == 2 && b > 0) { std::lock_guard<std::mutex> lk(j.m); j.done = a; j.total = b; }
                if (l == "step: shaders") step(2);
                else if (l == "step: copy") step(3);
                else if (l.rfind("error: ", 0) == 0)
                {
                    const std::string e = l.substr(7);
                    fail(e == "noipa" ? "Couldn't find FullDex for iPhone. Download the iPhone version of FullDex (the .ipa file) to this PC's Downloads folder and try again."
                         : e == "noengine" ? "This FullDex for iPhone has no 3DS games yet. Download the newer one to this PC's Downloads folder and try again."
                         : e == "ipa" ? "Couldn't add it to FullDex for iPhone. Try again; if it fails again, please report it with this PC's log."
                         : e == "copy" ? "Couldn't copy it. Is the SD card still in, and not locked?" : e == "tools" ? "The tools are missing. Try again to download them."
                         : "Building it didn't work. Try again; if it fails again, please report it with this PC's log.");
                }
            }, j.job);
            if (rc != 0) fail("Building it didn't work. Try again; if it fails again, please report it with this PC's log.");
            std::lock_guard<std::mutex> l(j.m);
            j.missing = nMissing;
            j.dumpCopied = copyDump;
        }
        {
            std::lock_guard<std::mutex> l(j.m);
            j.screen = j.error.empty() ? 2 : 3;
        }
        j.running = false;
    });
}

// The note once, when it ends with the dialog hidden (Keep Using FullDex). "" otherwise.
inline std::string SetupNote()
{
    SetupJob& j = SJ();
    std::lock_guard<std::mutex> l(j.m);
    if (j.told || j.running || j.open) return "";
    j.told = true;
    std::string n = j.name;
    for (char& ch : n) ch = (char)toupper((unsigned char)ch);
    if (j.screen == 3) return "COULDN'T FINISH " + n + ": CHOOSE IT AGAIN IN THE GAME'S OPTIONS";
    return j.kind == 1 ? n + " IS READY FOR YOUR SWITCH" : j.kind == 3 ? n + " IS READY FOR YOUR IPHONE" : n + " IS READY FOR YOUR PHONE";
}
inline std::string SetupBusyNote()
{
    SetupJob& j = SJ();
    std::lock_guard<std::mutex> l(j.m);
    if (!j.running || j.open) return "";
    std::string n = j.name;
    for (char& ch : n) ch = (char)toupper((unsigned char)ch);
    if (j.kind == 3) return "MAKING " + n + " FOR YOUR IPHONE: THE GAME'S OPTIONS SHOW HOW IT IS GOING";
    return (j.kind == 1 ? "PUTTING " + n + " ON YOUR SWITCH" : "MAKING " + n + " FOR YOUR PHONE") + std::string(": THE GAME'S OPTIONS SHOW HOW IT IS GOING");
}

// What the dialog shows now (each frame while it is open): its title, words and rows, and the extras.
inline void SetupFill(Launcher::GlassFocus& F)
{
    SetupJob& j = SJ();
    if (j.screen == 0 && std::chrono::steady_clock::now() - j.lastScan > std::chrono::seconds(1)) SetupRefreshCards();
    std::lock_guard<std::mutex> l(j.m);
    Launcher::SetupView& v = Launcher::SetupShown();
    v = Launcher::SetupView();
    F.dItems.clear();
    F.dPaths.clear();
    const std::string n = j.name;
    const bool sw = j.kind == 1, ios = j.kind == 3;
    if (j.screen != 4) v.steps = sw ? std::vector<std::string> { "Pick Where It Goes", "Build", "Done" }
                               : std::vector<std::string> { "Build", ios ? "Copy To Your iPhone" : "Copy To Your Phone" };
    if (j.screen == 4)
    {
        F.dTitle = "Set Up " + n + " On A Windows PC";
        F.dMsg = SetupHowTo(n);
        F.dItems = { "OK" }; F.dPaths = { "#close" };
        return;
    }
    if (j.screen == 0)
    {
        v.step = 0;
        F.dTitle = "Put " + n + " On Your Switch";
        F.dMsg = "Take the SD card out of your Switch and put it in this PC: FullDex finds it and does the rest. It uses your own dump and takes 5 to 10 minutes."
                 + std::string(SetupHaveTools(j.appDir) ? "" : "  The first time, FullDex also downloads the Switch tools (about 90 MB).")
                 + (j.cards.empty() ? "  No SD card found yet: put it in and it shows up here." : "");
        for (const SdCard& c : j.cards)
        {
            char t[200];
            snprintf(t, sizeof t, c.label.empty() ? "SD Card (%s)" : "%s (%s)", c.label.empty() ? c.root.c_str() : c.label.c_str(), c.root.c_str());
            F.dItems.push_back(t);
            F.dPaths.push_back("#card:" + c.root);
        }
        F.dItems.push_back("Save To A Folder Instead"); F.dPaths.push_back("#folder");
        F.dItems.push_back("Cancel"); F.dPaths.push_back("#close");
        return;
    }
    if (j.screen == 1)
    {
        v.step = sw ? 1 : 0;
        F.dTitle = sw ? "Building " + n + " For Your Switch" : ios ? "Building " + n + " For Your iPhone" : "Building " + n + " For Your Phone";
        F.dMsg = sw && !j.toFolder ? "Leave the SD card in. You can keep using FullDex: the game's options show how it is going."
                                   : "You can keep using FullDex: the game's options show how it is going.";
        auto row = [&](const std::string& t, int s) { v.list.push_back({ t, j.step > s ? 2 : j.step == s ? 1 : 0 }); };
        if (j.needTools) row(sw ? "Getting the Switch tools (once)" : "Getting the tools (once)", 0);
        row("Building the game from your dump", 1);
        if (ios) row("Adding it to FullDex for iPhone", 3);
        if (sw)
        {
            row("Making the shaders", 2);
            row(j.toFolder ? "Saving it in a folder" : "Copying it to the SD card (" + j.drive + ")", 3);
        }
        // (how far: each step's usual minutes, the one under way by its time so far)
        const double est[4] = { j.needTools ? 1.0 : 0.0, 3.5, sw ? 1.5 : 0.0, sw && !j.toFolder ? 1.5 : ios ? 0.5 : 0.3 };
        double total = 0, doneMin = 0;
        for (int s = 0; s < 4; s++) total += est[s];
        for (int s = 0; s < j.step && s < 4; s++) doneMin += est[s];
        const double inStep = std::chrono::duration<double>(std::chrono::steady_clock::now() - j.stepAt).count() / 60.0;
        // (the build's own count when it gives one, else its time so far)
        if (j.step == 1 && j.total > 0) doneMin += est[1] * std::min(0.97, (double)j.done / j.total);
        else if (j.step < 4) doneMin += std::min(inStep, est[j.step] * 0.95);
        v.progress = total > 0 ? (float)std::min(0.99, doneMin / total) : 0.f;
        const int left = std::max(1, (int)std::lround(total - doneMin));
        v.under = left <= 1 ? "About a minute left" : "About " + std::to_string(left) + " minutes left";
        F.dItems = { "Keep Using FullDex", "Stop" };
        F.dPaths = { "#hide", "#stop" };
        return;
    }
    if (j.screen == 2)
    {
        v.step = sw ? 2 : 1;
        if (sw && !j.toFolder)
        {
            F.dTitle = n + " Is On Your SD Card";
            F.dMsg = "Now, on your Switch: put the SD card back in, open FullDex and pick " + n + " in the 3DS Beta tab."
                     + (j.missing > 0 ? "  Also added " + std::to_string(j.missing) + " shaders your Switch found while you played, so those spots no longer stutter." : "")
                     + "  Next time you put the card in this PC, Put On My Switch picks up any new shaders.";
            F.dItems = { "Done" }; F.dPaths = { "#close" };
        }
        else if (sw)
        {
            F.dTitle = n + " Is Ready For Your Switch";
            F.dMsg = "Copy the folder " + SetupFolderName(j.code) + " into /switch/ on your Switch's SD card (it has the game and your dump in it).  Then open FullDex on your Switch and pick " + n + " in the 3DS Beta tab.";
            F.dItems = { "Open The Folder", "Done" }; F.dPaths = { "#open", "#close" };
        }
        else if (ios)
        {
            F.dTitle = n + " For Your iPhone Is Ready";
            F.dMsg = "It is in the folder that opens: FullDex for iPhone (3DS games built in).ipa, FullDex for iPhone and iPad with the game built in."
                     "  Install it on your iPhone or iPad the way you installed FullDex, over the one you have: your games and saves stay."
                     "  Then " + n + " runs at full speed.";
            F.dItems = { "Open The Folder", "Done" }; F.dPaths = { "#open", "#close" };
        }
        else
        {
            F.dTitle = n + " For Your Phone Is Ready";
            F.dMsg = "Copy " + j.id + ".so to your phone, into the same folder as your ROMs (USB cable, Google Drive, anything).  FullDex on your phone finds it by itself and the game runs at full speed.";
            F.dItems = { "Open The Folder", "Done" }; F.dPaths = { "#open", "#close" };
        }
        return;
    }
    F.dTitle = "Couldn't Finish";
    F.dMsg = j.error + (sw && !j.toFolder ? "  Nothing else on your SD card was changed." : "");
    F.dItems = { "Try Again", "Close" };
    F.dPaths = { "#again", "#close" };
}

// A row chosen in the dialog: true while it stays open.
inline bool SetupPick(const std::string& pick)
{
    SetupJob& j = SJ();
    if (pick.rfind("#card:", 0) == 0) { SetupStart(pick.substr(6), false); return true; }
    if (pick == "#folder") { SetupStart("", true); return true; }
    if (pick == "#hide") { std::lock_guard<std::mutex> l(j.m); j.open = false; return false; }
    if (pick == "#stop")
    {
        // (the build stopped: every process it started ended with it)
        std::lock_guard<std::mutex> l(j.m);
        if (j.running) { j.error = "Stopped. Nothing was finished."; if (j.job) TerminateJobObject(j.job, 1); }
        j.open = false;
        j.told = true;
        return false;
    }
    if (pick == "#open")
    {
        const std::string dir = j.kind != 1 ? j.out : std::filesystem::u8path(j.out).parent_path().u8string();
        ShellExecuteW(nullptr, L"open", W(Slashes(dir)).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        return true;
    }
    if (pick == "#again")
    {
        int kind;
        {
            std::lock_guard<std::mutex> l(j.m);
            j.screen = j.kind == 1 ? 0 : 1;
            j.error.clear();
            kind = j.kind;
        }
        if (kind != 1) SetupStart("", false);
        return true;
    }
    std::lock_guard<std::mutex> l(j.m);
    j.open = false;
    if (!j.running) j.told = true;
    return false;
}
