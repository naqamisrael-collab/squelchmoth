// Only used when cross-compiling from Linux with osxcross, which ships no compiler-rt:
// clang emits calls to __isPlatformVersionAtLeast for @available checks. This mirrors
// compiler-rt's implementation on macOS 10.15+ (libSystem provides _availability_version_check).
#include <stdint.h>

typedef struct { uint32_t platform; uint32_t version; } dyld_build_version_t;
extern int _availability_version_check (uint32_t count, dyld_build_version_t versions[]) __attribute__((weak_import));

int32_t __isPlatformVersionAtLeast (uint32_t platform, uint32_t major, uint32_t minor, uint32_t subminor)
{
    dyld_build_version_t v = { platform, (major << 16) | (minor << 8) | subminor };
    if (_availability_version_check != 0)
        return _availability_version_check (1, &v);
    return 0;
}

int32_t __isOSVersionAtLeast (uint32_t major, uint32_t minor, uint32_t subminor)
{
    return __isPlatformVersionAtLeast (1 /* PLATFORM_MACOS */, major, minor, subminor);
}
