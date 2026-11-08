#!/bin/bash
# Package the Windows build (build-win, cross-compiled with win-toolchain-clangcl.cmake) into
#   dist/Squelchmoth-Setup-<version>.exe   the installer (NSIS)
#   dist/Squelchmoth-Windows.zip           the installer + START HERE + the legal pack (what Payhip delivers)
# then runs the same owner-key / store-key / matched-name audit as the Mac packaging.
set -euo pipefail
cd "$(dirname "$0")/.."
VERSION=$(grep -m1 'project (' CMakeLists.txt | sed 's/.*VERSION \([0-9.]*\).*/\1/')
EXE=build-win/Squelchmoth_artefacts/Release/Squelchmoth.exe
[ -f "$EXE" ] || { echo "no Windows build at $EXE"; exit 1; }
CFV=$(grep -m1 'pinnedVersion =' Source/Tunnel.h | sed 's/.*"\(.*\)".*/\1/')

STAGE=$(mktemp -d)
cp "$EXE" "$STAGE/Squelchmoth.exe"
CFCACHE=dist/.cache/cloudflared-windows-amd64-$CFV.exe
mkdir -p dist/.cache
[ -f "$CFCACHE" ] || curl -fsSL -o "$CFCACHE" "https://github.com/cloudflare/cloudflared/releases/download/$CFV/cloudflared-windows-amd64.exe"
cp "$CFCACHE" "$STAGE/cloudflared.exe"
python3 -c "
from PIL import Image; im=Image.open('Resources/icon_1024.png').convert('RGBA')
im.save('$STAGE/squelchmoth.ico', sizes=[(16,16),(24,24),(32,32),(48,48),(64,64),(128,128),(256,256)])"
cp scripts/pkg/license.txt docs/legal/PRIVACY.txt docs/legal/THIRD-PARTY-NOTICES.txt "docs/START HERE.txt" docs/SETUP.md docs/PLUGINS.md "$STAGE/"
makensis -V2 -DVERSION="$VERSION" -DSRC="$STAGE" -DOUTFILE="$PWD/dist/Squelchmoth-Setup-$VERSION.exe" scripts/installer/windows/squelchmoth.nsi

Z=$(mktemp -d); mkdir -p "$Z/Squelchmoth-Windows/Legal"
cp "dist/Squelchmoth-Setup-$VERSION.exe" "$Z/Squelchmoth-Windows/"
cp "docs/START HERE.txt" "$Z/Squelchmoth-Windows/"
cp scripts/pkg/license.txt "$Z/Squelchmoth-Windows/Legal/LICENCE AGREEMENT (EULA).txt"
cp docs/legal/PRIVACY.txt docs/legal/THIRD-PARTY-NOTICES.txt "$Z/Squelchmoth-Windows/Legal/"
rm -f dist/Squelchmoth-Windows.zip
(cd "$Z" && zip -qr "$OLDPWD/dist/Squelchmoth-Windows.zip" Squelchmoth-Windows)
ls -la "dist/Squelchmoth-Setup-$VERSION.exe" dist/Squelchmoth-Windows.zip

# Audit: no owner secrets, store keys or matched-product names in what a customer downloads
LEAK=0
if unzip -l dist/Squelchmoth-Windows.zip | grep -Eiq 'OWNER KEY|SIGNING KEY|store keys|issued-serials|KEYS\.txt|BUSINESS\.md|SELLING-CHECKLIST|SUPER-PROMPT|COVERAGE-MAP|PARITY|STATUS|private'; then echo "LEAK: owner file in the Windows zip"; LEAK=1; fi
for f in "$STAGE"/*.txt "$STAGE"/*.md; do
  if grep -IlE -e 'SQM2-[A-Za-z0-9_-]{80,}' -e 'sk-ant-[A-Za-z0-9_-]{20,}' -e 'PRIVATE:' -e 'SIGNING KEY' -e 'PRIVATE KEY' -e 'SQM1-[A-Za-z0-9_-]{40,}' "$f" >/dev/null; then echo "LEAK: key text in $f"; LEAK=1; fi
done
for SK in /tmp/claude-0/scratch/storekey/private.hex; do [ -f "$SK" ] && grep -qaF "$(tr -d ' \n' < "$SK")" "$STAGE/Squelchmoth.exe" && { echo "LEAK: store signing key in the exe"; LEAK=1; }; done
if [ -f docs/internal/brand-audit-binary.txt ] && strings -n 5 "$STAGE/Squelchmoth.exe" | grep -Eq "$(cat docs/internal/brand-audit-binary.txt)"; then echo "LEAK: a matched-product name in Squelchmoth.exe"; LEAK=1; fi
if [ -f docs/internal/brand-audit-docs.txt ] && grep -IlE "$(cat docs/internal/brand-audit-docs.txt)" "$STAGE"/*.md "$STAGE"/*.txt >/dev/null 2>&1; then echo "LEAK: a matched-product name in the Windows guides"; LEAK=1; fi
rm -rf "$STAGE" "$Z"
[ $LEAK = 0 ] && echo "Audit: no owner keys, store keys, private documents or matched-product names in the Windows download" || exit 1
