#!/bin/bash
# Install Squelchmoth - (c) 2026 Jamar Stewart. All rights reserved.
# Copies Squelchmoth.app and Uninstall Squelchmoth.app from this download folder into /Applications
# (or ~/Applications when /Applications is not writable), clears the download quarantine flag on the copies
# so macOS opens them normally, and offers to launch Squelchmoth. No compiled code: runs on Intel and Apple Silicon.
#
#   --dry-run   prints what would happen and changes nothing (for testing)

TITLE="Install Squelchmoth"
DRY=""; [ "$1" = "--dry-run" ] && DRY=1
run () { if [ -n "$DRY" ]; then echo "would run: $*"; else "$@"; fi; }
say () { if [ -n "$DRY" ]; then echo "message: $1"; else osascript -e "display dialog \"$1\" buttons {\"OK\"} default button \"OK\" with title \"$TITLE\"" >/dev/null 2>&1; fi; }
ask () { if [ -n "$DRY" ]; then echo "${3}"; else osascript -e "display dialog \"$1\" buttons {$2} default button \"$3\" cancel button \"Cancel\" with title \"$TITLE\"" -e 'button returned of result' 2>/dev/null; fi; }

# The folder this installer sits in (inside the .app bundle: Contents/MacOS/.. /.. /..).
# macOS "App Translocation": an app opened straight out of a downloaded (quarantined) folder runs from a random
# read-only copy under /private/var/folders/.../AppTranslocation/, so nothing sits next to it. Find the real folder:
#   1. ask macOS for the original path (Security framework, through JavaScript for Automation - no compiled code)
#   2. look for the unzipped download in Downloads / Desktop / Documents (newest first)
#   3. let the user point at the folder
HERE="$(cd "$(dirname "$0")/../../.." && pwd)"
SELF_APP="$(cd "$(dirname "$0")/../.." && pwd)"
has_app () { [ -d "$1/Squelchmoth.app" ]; }
if ! has_app "$HERE"; then
    ORIG=$(osascript -l JavaScript -e '
        ObjC.import("Foundation"); ObjC.import("Security");
        ObjC.bindFunction("SecTranslocateCreateOriginalPathForURL", ["id", ["id", "void *"]]);
        var u = $.NSURL.fileURLWithPath("'"$SELF_APP"'");
        var o = $.SecTranslocateCreateOriginalPathForURL(u, null);
        (o && !o.isNil()) ? ObjC.unwrap(o.path) : "";' 2>/dev/null)
    [ -n "$ORIG" ] && has_app "$(dirname "$ORIG")" && HERE="$(dirname "$ORIG")"
fi
if ! has_app "$HERE"; then
    while IFS= read -r inst; do
        d="$(dirname "$inst")"; if has_app "$d"; then HERE="$d"; break; fi
    done < <(find "$HOME/Downloads" "$HOME/Desktop" "$HOME/Documents" -maxdepth 4 -type d -name "Install Squelchmoth.app" -not -path "*/AppTranslocation/*" 2>/dev/null \
             | while IFS= read -r f; do echo "$(stat -f %m "$f" 2>/dev/null || echo 0) $f"; done | sort -rn | cut -d" " -f2-)
fi
if ! has_app "$HERE" && [ -z "$DRY" ]; then
    PICK=$(osascript -e 'POSIX path of (choose folder with prompt "Squelchmoth could not find its download folder. Choose the unzipped Squelchmoth folder (the one with Squelchmoth and Install Squelchmoth in it):")' 2>/dev/null)
    PICK="${PICK%/}"; [ -n "$PICK" ] && has_app "$PICK" && HERE="$PICK"
fi
SRC_APP="$HERE/Squelchmoth.app"
SRC_UN="$HERE/Uninstall Squelchmoth.app"
if [ ! -d "$SRC_APP" ]; then
    say "Squelchmoth.app was not found.\n\nOpen the unzipped Squelchmoth folder, drag Squelchmoth into Applications yourself (the Applications folder in the Finder sidebar), then right-click it there and choose Open."
    exit 1
fi

DEST="/Applications"
[ -w "$DEST" ] || { DEST="$HOME/Applications"; run mkdir -p "$DEST"; }

CHOICE=$(ask "This copies Squelchmoth into $DEST (replacing an older version if there is one) and keeps your sessions, settings and licence.\n\nInstall now?" '"Cancel", "Install"' "Install") || exit 0
[ "$CHOICE" = "Install" ] || exit 0

pgrep -x Squelchmoth >/dev/null 2>&1 && run osascript -e 'tell application "Squelchmoth" to quit' >/dev/null 2>&1
sleep 1

# Replace both apps. An older copy put there by the .pkg installer belongs to the system (root), so a plain user
# can't delete it: try as the user first, and if anything is left over do the whole step once with the Mac's own
# administrator password prompt. "ditto" copies into an exact path (cp -R would nest into a folder it couldn't remove).
q () { printf "'%s'" "$(printf %s "$1" | sed "s/'/'\\\\''/g")"; }
STEP="rm -rf $(q "$DEST/Squelchmoth.app") && ditto $(q "$SRC_APP") $(q "$DEST/Squelchmoth.app") && { xattr -dr com.apple.quarantine $(q "$DEST/Squelchmoth.app") 2>/dev/null; true; }"
[ -d "$SRC_UN" ] && STEP="$STEP && rm -rf $(q "$DEST/Uninstall Squelchmoth.app") && ditto $(q "$SRC_UN") $(q "$DEST/Uninstall Squelchmoth.app") && { xattr -dr com.apple.quarantine $(q "$DEST/Uninstall Squelchmoth.app") 2>/dev/null; true; }"
installed_ok () { [ -x "$DEST/Squelchmoth.app/Contents/MacOS/Squelchmoth" ] && { [ ! -d "$SRC_UN" ] || [ -x "$DEST/Uninstall Squelchmoth.app/Contents/MacOS/Uninstall Squelchmoth" ]; } \
                  && [ ! -d "$DEST/Squelchmoth.app/Squelchmoth.app" ] && [ ! -d "$DEST/Uninstall Squelchmoth.app/Uninstall Squelchmoth.app" ]; }
if [ -n "$DRY" ]; then echo "would run: $STEP"
else
    if ! bash -c "$STEP" 2>/dev/null || ! installed_ok; then
        ESC=$(printf %s "$STEP" | sed 's/\\/\\\\/g; s/"/\\"/g')
        osascript -e "do shell script \"$ESC\" with administrator privileges with prompt \"Squelchmoth needs your Mac password once to replace the older copy in Applications.\"" >/dev/null 2>&1
    fi
    installed_ok || { say "Couldn't replace Squelchmoth in $DEST.\n\nDrag the old Squelchmoth and Uninstall Squelchmoth from Applications to the Trash, then run Install Squelchmoth again."; exit 1; }
fi

OPEN=$(ask "Squelchmoth is installed in $DEST.\n\nOpen it now?" '"Later", "Open Squelchmoth"' "Open Squelchmoth") || exit 0
[ "$OPEN" = "Open Squelchmoth" ] && run open "$DEST/Squelchmoth.app"
exit 0
