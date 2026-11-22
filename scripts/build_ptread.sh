#!/bin/bash
# Builds sqptread (tools/sqptread + ThirdParty/ptformat, LGPL) for every platform into dist/.helpers/. It is a separate
# program on purpose: the app runs it and never links the library.
set -e
ROOT="$(cd "$(dirname "$0")/.." && pwd)"; cd "$ROOT"; OUT=dist/.helpers; mkdir -p "$OUT"
SRC="ThirdParty/ptformat/ptformat.cc tools/sqptread/main.cpp"; INC="-IThirdParty/ptformat"
g++ -O2 -std=c++11 $INC $SRC -o "$OUT/sqptread-linux"
X=/home/claude/xwin
clang-cl-18 --target=x86_64-pc-windows-msvc -fuse-ld=lld-link /imsvc$X/crt/include /imsvc$X/sdk/include/ucrt /imsvc$X/sdk/include/um /imsvc$X/sdk/include/shared /EHsc /O2 /MT -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -Wno-unused-command-line-argument \
    /IThirdParty/ptformat $SRC /Fo"$OUT/" /Fe"$OUT/sqptread.exe" /link /libpath:$X/crt/lib/x86_64 /libpath:$X/sdk/lib/um/x86_64 /libpath:$X/sdk/lib/ucrt/x86_64 /machine:x64 >/dev/null
rm -f "$OUT"/*.obj
Z=/home/claude/zigmac/bin; SDK=/home/claude/macsdk/MacOSX13.3.sdk
$Z/x86_64-apple-darwin-clang++ -O2 -std=c++11 -mmacosx-version-min=11.0 -isysroot $SDK $INC $SRC -o "$OUT/sqptread-mac-x86_64"
$Z/arm64-apple-darwin-clang++  -O2 -std=c++11 -mmacosx-version-min=11.0 -isysroot $SDK $INC $SRC -o "$OUT/sqptread-mac-arm64"
/usr/lib/llvm-18/bin/llvm-lipo -create "$OUT/sqptread-mac-x86_64" "$OUT/sqptread-mac-arm64" -output "$OUT/sqptread-mac-universal"
(cd "$ROOT" && rm -f "$OUT/sqptread-source.zip" && zip -qr "$OUT/sqptread-source.zip" ThirdParty/ptformat tools/sqptread scripts/build_ptread.sh)
ls -la "$OUT"
