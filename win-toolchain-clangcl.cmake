# Cross-compile Squelchmoth for Windows x64 from Linux: clang-cl + lld-link (LLVM 18) against the MSVC CRT and the
# Windows SDK fetched by xwin (Microsoft's build-tools licence, accepted by Jamar Stewart on Nov 6 2026).
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_VERSION 10.0)
set(CMAKE_SYSTEM_PROCESSOR AMD64)
set(XWIN /home/claude/xwin)

set(CMAKE_C_COMPILER   /usr/bin/clang-cl-18)
set(CMAKE_CXX_COMPILER /usr/bin/clang-cl-18)
set(CMAKE_LINKER       /usr/bin/lld-link-18)
set(CMAKE_AR           /usr/bin/llvm-lib-18)
set(CMAKE_RC_COMPILER  /usr/bin/llvm-rc-18)
set(CMAKE_MT           /usr/bin/llvm-mt-18)

set(_inc "/imsvc${XWIN}/crt/include /imsvc${XWIN}/sdk/include/ucrt /imsvc${XWIN}/sdk/include/um /imsvc${XWIN}/sdk/include/shared /imsvc${XWIN}/sdk/include/winrt /imsvc${XWIN}/sdk/include/cppwinrt")
set(CMAKE_C_FLAGS_INIT   "--target=x86_64-pc-windows-msvc -fuse-ld=lld-link ${_inc} -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -D_USE_MATH_DEFINES")
set(CMAKE_CXX_FLAGS_INIT "--target=x86_64-pc-windows-msvc -fuse-ld=lld-link ${_inc} /EHsc -D_ALLOW_COMPILER_AND_STL_VERSION_MISMATCH -D_USE_MATH_DEFINES -Wno-unused-command-line-argument")
set(CMAKE_RC_FLAGS_INIT  "-I${XWIN}/sdk/include/um -I${XWIN}/sdk/include/shared")

set(_lib "/libpath:${XWIN}/crt/lib/x86_64 /libpath:${XWIN}/sdk/lib/um/x86_64 /libpath:${XWIN}/sdk/lib/ucrt/x86_64")
set(CMAKE_EXE_LINKER_FLAGS_INIT    "${_lib}")
set(CMAKE_SHARED_LINKER_FLAGS_INIT "${_lib}")
set(CMAKE_MODULE_LINKER_FLAGS_INIT "${_lib}")

set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
# Static CRT (/MT): the app runs without the Visual C++ redistributable. No debug CRT is fetched, so try-compiles use Release.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")
set(CMAKE_TRY_COMPILE_CONFIGURATION Release)
set(CMAKE_POLICY_DEFAULT_CMP0091 NEW)

# identical floating-point rounding on every platform (no fused multiply-add): DSP measures the same on Apple Silicon as on Intel
set(CMAKE_C_FLAGS_INIT "${CMAKE_C_FLAGS_INIT} /clang:-ffp-contract=off")
set(CMAKE_CXX_FLAGS_INIT "${CMAKE_CXX_FLAGS_INIT} /clang:-ffp-contract=off")
