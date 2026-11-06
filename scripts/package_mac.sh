#!/bin/bash
# Packages the macOS app from the two osxcross builds (build-mac = x86_64, build-mac-arm64 = arm64).
#
# Produces in dist/ (the per-architecture zips carry an Installer package that puts the app in Applications):
#   Squelchmoth-macOS.zip              universal app (Intel + Apple Silicon), full-size cloudflared   (~60 MB)
#   Squelchmoth-Mac-Intel.zip          Intel-only app, symbols stripped, Intel cloudflared            (~23 MB)
#   Squelchmoth-Mac-AppleSilicon.zip   Apple Silicon-only app, stripped, arm64 cloudflared          (~22 MB)
# The per-architecture zips exist so the download stays under 30 MB; they are functionally identical.
# Every app bundles cloudflared in Contents/Resources (no download needed for the Claude link), the docs,
# and is ad-hoc signed with rcodesign (no hardened runtime, so third-party plugins load).
set -e
cd "$(dirname "$0")/.."
export PATH=/home/claude/osxcross/target/bin:$PATH

X86=build-mac/Squelchmoth_artefacts/Release/Squelchmoth.app
ARM=build-mac-arm64/Squelchmoth_artefacts/Release/Squelchmoth.app
CF=/home/claude/cf
# customer-facing docs only: BUSINESS.md and the owner key file are delivered to the owner separately
DOCS=(docs/SETUP.md docs/PLUGINS.md "docs/START HERE.txt")   # docs/internal never ships
# legal pack: shipped in the app (Help menu) and in every download
LEGAL=(scripts/pkg/license.txt docs/legal/PRIVACY.txt docs/legal/THIRD-PARTY-NOTICES.txt)
VERSION=$(grep -m1 'project (' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')

# "Uninstall Squelchmoth.app": a small app bundle around scripts/uninstaller/uninstall.sh (no compiled code, so it runs
# on Intel and Apple Silicon alike). Installed next to Squelchmoth.app by the Installer package.
make_uninstaller () {   # $1 = output folder
    local U="$1/Uninstall Squelchmoth.app"
    rm -rf "$U"; mkdir -p "$U/Contents/MacOS" "$U/Contents/Resources"
    cp scripts/uninstaller/uninstall.sh "$U/Contents/MacOS/Uninstall Squelchmoth"; chmod 755 "$U/Contents/MacOS/Uninstall Squelchmoth"
    cp "$X86/Contents/Resources/AppIcon.icns" "$U/Contents/Resources/AppIcon.icns"
    cat > "$U/Contents/Info.plist" <<PLIST
<?xml version="1.0" encoding="UTF-8"?>
<!DOCTYPE plist PUBLIC "-//Apple//DTD PLIST 1.0//EN" "http://www.apple.com/DTDs/PropertyList-1.0.dtd">
<plist version="1.0"><dict>
  <key>CFBundleExecutable</key><string>Uninstall Squelchmoth</string>
  <key>CFBundleIdentifier</key><string>com.jamarstewart.squelchmoth.uninstaller</string>
  <key>CFBundleName</key><string>Uninstall Squelchmoth</string>
  <key>CFBundleDisplayName</key><string>Uninstall Squelchmoth</string>
  <key>CFBundleIconFile</key><string>AppIcon</string>
  <key>CFBundlePackageType</key><string>APPL</string>
  <key>CFBundleShortVersionString</key><string>$VERSION</string>
  <key>CFBundleVersion</key><string>$VERSION</string>
  <key>LSMinimumSystemVersion</key><string>11.0</string>
  <key>NSHumanReadableCopyright</key><string>(c) 2026 Jamar Stewart. All rights reserved.</string>
</dict></plist>
PLIST
    printf 'APPL????' > "$U/Contents/PkgInfo"
}

# cloudflared per architecture: full copies and stripped copies (local symbols only; Go keeps its own tables)
CFX=/tmp/cfx; rm -rf "$CFX"; mkdir -p "$CFX"
tar -xzf "$CF/cloudflared-darwin-amd64.tgz" -C "$CFX" && mv "$CFX/cloudflared" "$CFX/cf-amd64"
tar -xzf "$CF/cloudflared-darwin-arm64.tgz" -C "$CFX" && mv "$CFX/cloudflared" "$CFX/cf-arm64"
for a in amd64 arm64; do cp "$CFX/cf-$a" "$CFX/cf-$a-stripped"; llvm-strip -x "$CFX/cf-$a-stripped"; done
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
    [ -d "dist/$1/Squelchmoth.app" ] && make_uninstaller "dist/$1"   # the drag-install zip gets the uninstaller too
    rm -f "dist/$1/OWNER KEY - keep private.txt" "dist/$1/BUSINESS.md"   # never ship owner secrets in a download
    (cd dist && rm -f "$2" && zip -qry "$2" "$1")
    ls -la "dist/$2"
}

# make_pkg: a macOS Installer package (Installer.app) that puts Squelchmoth.app into /Applications.
# Built with bomutils (mkbom) + xar, the same flat-package format pkgbuild/productbuild write.
#   $1 = app to install, $2 = output .pkg, $3 = host architecture (x86_64 / arm64 / "x86_64,arm64")
make_pkg () {
    local APP="$1" PKG="$2" ARCH="$3"
    local VER; VER=$(grep -A1 CFBundleShortVersionString "$APP/Contents/Info.plist" | tail -1 | sed 's/.*<string>\(.*\)<\/string>.*/\1/')
    local W=/tmp/sqm-pkg; rm -rf "$W"; mkdir -p "$W/root" "$W/flat/app.pkg" "$W/flat/Resources/en.lproj"
    # payload root = /Applications itself (install-location); it holds only the app, and existing folder
    # permissions are never touched (overwrite-permissions="false"), exactly like pkgbuild --component
    chmod 775 "$W/root"
    cp -R "$APP" "$W/root/Squelchmoth.app"
    make_uninstaller "$W/root"
    local FILES KB
    FILES=$(find "$W/root" | wc -l | tr -d ' ')
    KB=$(du -sk "$W/root" | cut -f1)
    (cd "$W/root" && find . | LC_ALL=C sort | cpio -o --quiet --format odc --owner 0:80 | gzip -9 -c) > "$W/flat/app.pkg/Payload"
    mkbom -u 0 -g 80 "$W/root" "$W/flat/app.pkg/Bom" >/dev/null
    cat > "$W/flat/app.pkg/PackageInfo" <<XML
<?xml version="1.0" encoding="utf-8"?>
<pkg-info overwrite-permissions="false" relocatable="false" identifier="com.jamarstewart.squelchmoth.app" postinstall-action="none" version="$VER" format-version="2" generator-version="Squelchmoth make_pkg" install-location="/Applications" auth="root">
    <payload numberOfFiles="$FILES" installKBytes="$KB"/>
    <bundle path="./Squelchmoth.app" id="com.jamarstewart.squelchmoth" CFBundleShortVersionString="$VER" CFBundleVersion="$VER"/>
    <bundle path="./Uninstall Squelchmoth.app" id="com.jamarstewart.squelchmoth.uninstaller" CFBundleShortVersionString="$VER" CFBundleVersion="$VER"/>
    <bundle-version>
        <bundle id="com.jamarstewart.squelchmoth"/>
    </bundle-version>
    <upgrade-bundle>
        <bundle id="com.jamarstewart.squelchmoth"/>
    </upgrade-bundle>
    <update-bundle/>
    <atomic-update-bundle/>
    <strict-identifier/>
    <relocate/>
</pkg-info>
XML
    cat > "$W/flat/Distribution" <<XML
<?xml version="1.0" encoding="utf-8"?>
<installer-gui-script minSpecVersion="2">
    <title>Squelchmoth $VER</title>
    <organization>com.jamarstewart</organization>
    <options customize="never" require-scripts="false" hostArchitectures="$ARCH" rootVolumeOnly="true"/>
    <domains enable_anywhere="false" enable_currentUserHome="false" enable_localSystem="true"/>
    <welcome file="welcome.html" mime-type="text/html"/>
    <readme file="readme.html" mime-type="text/html"/>
    <license file="license.txt" mime-type="text/plain"/>
    <conclusion file="conclusion.html" mime-type="text/html"/>
    <allowed-os-versions><os-version min="11.0"/></allowed-os-versions>
    <choices-outline>
        <line choice="default">
            <line choice="com.jamarstewart.squelchmoth.app"/>
        </line>
    </choices-outline>
    <choice id="default" title="Squelchmoth"/>
    <choice id="com.jamarstewart.squelchmoth.app" visible="false" title="Squelchmoth" description="Squelchmoth.app and Uninstall Squelchmoth.app in Applications">
        <pkg-ref id="com.jamarstewart.squelchmoth.app"/>
    </choice>
    <pkg-ref id="com.jamarstewart.squelchmoth.app" version="$VER" onConclusion="none" installKBytes="$KB">#app.pkg</pkg-ref>
</installer-gui-script>
XML
    cp scripts/pkg/welcome.html scripts/pkg/readme.html scripts/pkg/conclusion.html scripts/pkg/license.txt "$W/flat/Resources/en.lproj/"
    rm -f "$PKG"
    (cd "$W/flat" && xar --compression none -cf "$OLDPWD/$PKG" Distribution Resources app.pkg)
    ls -la "$PKG"
}

mkdir -p dist

# 1. universal
OUT=dist/Squelchmoth; rm -rf "$OUT"; mkdir -p "$OUT"
cp -R "$X86" "$OUT/Squelchmoth.app"
lipo -create "$X86/Contents/MacOS/Squelchmoth" "$ARM/Contents/MacOS/Squelchmoth" -output "$OUT/Squelchmoth.app/Contents/MacOS/Squelchmoth"
lipo -info "$OUT/Squelchmoth.app/Contents/MacOS/Squelchmoth"
finish_bundle "$OUT/Squelchmoth.app" "$CFX/cf-universal"
make_zip Squelchmoth Squelchmoth-macOS.zip

# 2. Intel (stripped) - the download holds the Installer package (installs into Applications)
STAGE=/tmp/sqm-stage; rm -rf "$STAGE"; mkdir -p "$STAGE"
cp -R "$X86" "$STAGE/Squelchmoth.app"
llvm-strip -x "$STAGE/Squelchmoth.app/Contents/MacOS/Squelchmoth"
finish_bundle "$STAGE/Squelchmoth.app" "$CFX/cf-amd64-stripped"
OUT=dist/Squelchmoth-Intel; rm -rf "$OUT"; mkdir -p "$OUT"
make_pkg "$STAGE/Squelchmoth.app" "$OUT/Install Squelchmoth.pkg" x86_64
make_zip Squelchmoth-Intel Squelchmoth-Mac-Intel.zip

# 3. Apple Silicon (stripped)
rm -rf "$STAGE"; mkdir -p "$STAGE"
cp -R "$ARM" "$STAGE/Squelchmoth.app"
llvm-strip -x "$STAGE/Squelchmoth.app/Contents/MacOS/Squelchmoth"
finish_bundle "$STAGE/Squelchmoth.app" "$CFX/cf-arm64-stripped"
OUT=dist/Squelchmoth-AppleSilicon; rm -rf "$OUT"; mkdir -p "$OUT"
make_pkg "$STAGE/Squelchmoth.app" "$OUT/Install Squelchmoth.pkg" arm64
make_zip Squelchmoth-AppleSilicon Squelchmoth-Mac-AppleSilicon.zip

# 4. Universal installer (both architectures in one package)
make_pkg "dist/Squelchmoth/Squelchmoth.app" "dist/Install Squelchmoth (Universal).pkg" "x86_64,arm64"

# 5. Audit: no owner secrets in anything a customer downloads (fails the build if one slipped in)
LEAK=0
for z in dist/Squelchmoth-macOS.zip dist/Squelchmoth-Mac-Intel.zip dist/Squelchmoth-Mac-AppleSilicon.zip; do
    if unzip -l "$z" | grep -Eiq 'OWNER KEY|SIGNING KEY|store keys|issued-serials|KEYS\.txt|BUSINESS\.md|SELLING-CHECKLIST|SUPER-PROMPT|COVERAGE-MAP|PARITY|STATUS|private'; then echo "LEAK: owner file in $z"; LEAK=1; fi
    T=$(mktemp -d); unzip -q "$z" -d "$T"
    if grep -rIl --exclude='*.icns' -e 'PRIVATE KEY' -e 'owner-keygen' -e 'SQM1-[A-Za-z0-9_-]\{40,\}' "$T" 2>/dev/null | grep -v '\.pkg$' | head -1 | grep -q .; then echo "LEAK: key text inside $z"; LEAK=1; fi
    if grep -rIlE --include='*.txt' --include='*.md' --include='*.rtf' --include='*.json' --include='*.xml' --include='*.plist' --include='*.html' -e 'SQM2-[A-Za-z0-9_-]{80,}' -e 'PRIVATE:' -e 'SIGNING KEY' "$T" 2>/dev/null | head -1 | grep -q .; then echo "LEAK: store key text inside $z"; LEAK=1; fi
    # no matched-product name in the guides or the app's strings (Jamar, Nov 6 2026); the name lists live in docs/internal (never shipped)
    if [ -f docs/internal/brand-audit-docs.txt ]; then
    if grep -rIlE --include='*.md' --include='*.txt' "$(cat docs/internal/brand-audit-docs.txt)" "$T" 2>/dev/null | head -1 | grep -q .; then echo "LEAK: a matched-product name in the guides of $z"; LEAK=1; fi
    for b in $(find "$T" -path '*Contents/MacOS/Squelchmoth'); do if strings -n 5 "$b" | grep -Eq "$(cat docs/internal/brand-audit-binary.txt)"; then echo "LEAK: a matched-product name in $b"; LEAK=1; fi; done
    fi
    rm -rf "$T"
done
[ $LEAK = 0 ] && echo "Audit: no owner keys, private documents or matched-product names in the downloads" || exit 1
