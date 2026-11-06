#!/bin/bash
# Packages the macOS app from the two zig cross-builds (build-mac = x86_64, build-mac-arm64 = arm64).
# Same output as package_mac.sh minus the .pkg (no bomutils / xar on this machine): each zip holds the app,
# "Install Squelchmoth.app" (copies it into Applications), "Uninstall Squelchmoth.app" and the guides.
#
#   dist/Squelchmoth-macOS.zip              universal app (Intel + Apple Silicon), full-size cloudflared
#   dist/Squelchmoth-Mac-Intel.zip          Intel-only app, symbols stripped, Intel cloudflared
#   dist/Squelchmoth-Mac-AppleSilicon.zip   Apple Silicon-only app, stripped, arm64 cloudflared
# Every app bundles cloudflared in Contents/Resources, the docs, and is ad-hoc signed with rcodesign.
set -e
cd "$(dirname "$0")/.."
export PATH=/usr/lib/llvm-18/bin:/home/claude/osxcross/target/bin:$PATH
lipo () { llvm-lipo "$@"; }

X86=build-mac/Squelchmoth_artefacts/Release/Squelchmoth.app
ARM=build-mac-arm64/Squelchmoth_artefacts/Release/Squelchmoth.app
CF=/home/claude/cf
DOCS=(docs/SETUP.md docs/PLUGINS.md "docs/START HERE.txt")   # docs/internal never ships
LEGAL=(scripts/pkg/license.txt docs/legal/PRIVACY.txt docs/legal/THIRD-PARTY-NOTICES.txt)
VERSION=$(grep -m1 'project (' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')

# zig records its own SDK version in LC_BUILD_VERSION; stamp the SDK the headers came from (13.3) so AppKit's
# "linked on or after" checks behave like an Xcode 14 build. Pure byte patch, before signing.
stamp_sdk () {   # $1 = Mach-O (thin)
python3 - "$1" <<'PY'
import struct, sys
p = sys.argv[1]; d = bytearray(open(p, 'rb').read())
magic, cputype, cpusub, ft, ncmds, sizeofcmds, flags, res = struct.unpack_from('<IiiIIIII', d, 0)
assert magic == 0xfeedfacf, 'not a 64-bit Mach-O'
off = 32
for _ in range(ncmds):
    cmd, size = struct.unpack_from('<II', d, off)
    if cmd == 0x32:   # LC_BUILD_VERSION: platform, minos, sdk, ntools
        platform, minos, sdk = struct.unpack_from('<III', d, off + 8)
        struct.pack_into('<I', d, off + 16, (13 << 16) | (3 << 8))
        print(f'  {p}: minos {minos>>16}.{(minos>>8)&255} sdk {sdk>>16}.{(sdk>>8)&255} -> 13.3')
    off += size
open(p, 'wb').write(d)
PY
}

make_tool_app () {   # $1 = output folder, $2 = app name, $3 = script, $4 = bundle id suffix
    local U="$1/$2.app"
    rm -rf "$U"; mkdir -p "$U/Contents/MacOS" "$U/Contents/Resources"
    cp "$3" "$U/Contents/MacOS/$2"; chmod 755 "$U/Contents/MacOS/$2"
    cp "$X86/Contents/Resources/AppIcon.icns" "$U/Contents/Resources/AppIcon.icns"
    cat > "$U/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleExecutable</key><string>$2</string>
  <key>CFBundleIdentifier</key><string>com.jamarstewart.squelchmoth.$4</string>
  <key>CFBundleName</key><string>$2</string>
  <key>CFBundleDisplayName</key><string>$2</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHumanReadableCopyright</key><string>(c) 2026 Jamar Stewart. All rights reserved.</string>
</dict></plist>
PLIST
    printf 'APPL????' > "$U/Contents/PkgInfo"
    # a shell-script app can't carry a Mach-O code signature; macOS runs it after the usual right-click > Open
}

CFX=/tmp/cfx; rm -rf "$CFX"; mkdir -p "$CFX"
tar -xzf "$CF/cloudflared-darwin-amd64.tgz" -C "$CFX" && mv "$CFX/cloudflared" "$CFX/cf-amd64"
tar -xzf "$CF/cloudflared-darwin-arm64.tgz" -C "$CFX" && mv "$CFX/cloudflared" "$CFX/cf-arm64"
for a in amd64 arm64; do cp "$CFX/cf-$a" "$CFX/cf-$a-stripped"; done   # (llvm-strip 18 leaves a layout rcodesign rejects, so cloudflared ships unstripped)
lipo -create "$CFX/cf-amd64" "$CFX/cf-arm64" -output "$CFX/cf-universal"

finish_bundle () {   # $1 = app path, $2 = cloudflared binary to bundle
    local APP="$1"
    rm -rf "$APP/Contents/_CodeSignature"
    cp "$2" "$APP/Contents/Resources/cloudflared"; chmod +x "$APP/Contents/Resources/cloudflared"
    cp "${DOCS[@]}" "${LEGAL[@]}" scripts/uninstaller/uninstall.sh "$APP/Contents/Resources/"
    rcodesign sign "$APP/Contents/Resources/cloudflared" >/dev/null
    rcodesign sign "$APP" >/dev/null
}

make_zip () {        # $1 = folder name under dist, $2 = zip name
    cp "${DOCS[@]}" "dist/$1/"
    mkdir -p "dist/$1/Legal"; cp "${LEGAL[@]}" "dist/$1/Legal/"; mv "dist/$1/Legal/license.txt" "dist/$1/Legal/LICENCE AGREEMENT (EULA).txt"
    make_tool_app "dist/$1" "Install Squelchmoth" scripts/installer/install.sh installer
    make_tool_app "dist/$1" "Uninstall Squelchmoth" scripts/uninstaller/uninstall.sh uninstaller
    rm -f "dist/$1/OWNER KEY - keep private.txt" "dist/$1/BUSINESS.md"
    (cd dist && rm -f "$2" && zip -qry "$2" "$1")
    ls -la "dist/$2"
}

mkdir -p dist
echo "SDK stamps:"
stamp_sdk "$X86/Contents/MacOS/Squelchmoth"
stamp_sdk "$ARM/Contents/MacOS/Squelchmoth"

# 1. universal
OUT=dist/Squelchmoth; rm -rf "$OUT"; mkdir -p "$OUT"
cp -R "$X86" "$OUT/Squelchmoth.app"
lipo -create "$X86/Contents/MacOS/Squelchmoth" "$ARM/Contents/MacOS/Squelchmoth" -output "$OUT/Squelchmoth.app/Contents/MacOS/Squelchmoth"
lipo -info "$OUT/Squelchmoth.app/Contents/MacOS/Squelchmoth"
finish_bundle "$OUT/Squelchmoth.app" "$CFX/cf-universal"
make_zip Squelchmoth Squelchmoth-macOS.zip

# 2. Intel (stripped)
OUT=dist/Squelchmoth-Intel; rm -rf "$OUT"; mkdir -p "$OUT"
cp -R "$X86" "$OUT/Squelchmoth.app"
llvm-strip -x "$OUT/Squelchmoth.app/Contents/MacOS/Squelchmoth"
finish_bundle "$OUT/Squelchmoth.app" "$CFX/cf-amd64-stripped"
make_zip Squelchmoth-Intel Squelchmoth-Mac-Intel.zip

# 3. Apple Silicon (stripped)
OUT=dist/Squelchmoth-AppleSilicon; rm -rf "$OUT"; mkdir -p "$OUT"
cp -R "$ARM" "$OUT/Squelchmoth.app"
llvm-strip -x "$OUT/Squelchmoth.app/Contents/MacOS/Squelchmoth"
finish_bundle "$OUT/Squelchmoth.app" "$CFX/cf-arm64-stripped"
make_zip Squelchmoth-AppleSilicon Squelchmoth-Mac-AppleSilicon.zip

# 4. Audit: no owner secrets in anything a customer downloads (fails the build if one slipped in)
LEAK=0
for z in dist/Squelchmoth-macOS.zip dist/Squelchmoth-Mac-Intel.zip dist/Squelchmoth-Mac-AppleSilicon.zip; do
    if unzip -l "$z" | grep -Eiq 'OWNER KEY|SIGNING KEY|store keys|issued-serials|KEYS\.txt|BUSINESS\.md|SELLING-CHECKLIST|SUPER-PROMPT|COVERAGE-MAP|PARITY|STATUS|private'; then echo "LEAK: owner file in $z"; LEAK=1; fi
    T=$(mktemp -d); unzip -q "$z" -d "$T"
    if grep -rIl --exclude='*.icns' -e 'PRIVATE KEY' -e 'owner-keygen' -e 'SQM1-[A-Za-z0-9_-]\{40,\}' "$T" 2>/dev/null | head -1 | grep -q .; then echo "LEAK: key text inside $z"; LEAK=1; fi
    # 1.9 store keys: no SQM2 key, store signing key or key batch in any text file (the binary holds self-test vectors only)
    if grep -rIlE --include='*.txt' --include='*.md' --include='*.rtf' --include='*.json' --include='*.xml' --include='*.plist' --include='*.html' -e 'SQM2-[A-Za-z0-9_-]{80,}' -e 'PRIVATE:' -e 'SIGNING KEY' "$T" 2>/dev/null | head -1 | grep -q .; then echo "LEAK: store key text inside $z"; LEAK=1; fi
    for SK in /tmp/claude-0/scratch/storekey/private.hex; do [ -f "$SK" ] && grep -rIlaF "$(tr -d ' \n' < "$SK")" "$T" 2>/dev/null | head -1 | grep -q . && { echo "LEAK: the store signing key is inside $z"; LEAK=1; }; done
    # no matched-product name in the guides or the app's strings (Jamar, Nov 6 2026); the name lists live in docs/internal (never shipped)
    if [ -f docs/internal/brand-audit-docs.txt ]; then
    if grep -rIlE --include='*.md' --include='*.txt' "$(cat docs/internal/brand-audit-docs.txt)" "$T" 2>/dev/null | head -1 | grep -q .; then echo "LEAK: a matched-product name in the guides of $z"; LEAK=1; fi
    for b in $(find "$T" -path '*Contents/MacOS/Squelchmoth'); do if strings -n 5 "$b" | grep -Eq "$(cat docs/internal/brand-audit-binary.txt)"; then echo "LEAK: a matched-product name in $b"; LEAK=1; fi; done
    fi
    rm -rf "$T"
done
[ $LEAK = 0 ] && echo "Audit: no owner keys, private documents or matched-product names in the downloads" || exit 1
